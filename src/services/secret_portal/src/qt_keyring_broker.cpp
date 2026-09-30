// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/secret_portal/secret_broker.h>
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <qindaqt/services/secret_portal/legacy_import.h>
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QScopeGuard>
#include <QTimer>
#include <QUuid>
#include <map>
#include <unistd.h>
namespace QindaQt::Services::SecretPortal {
namespace {
using qindaqt::keyring::protocol::argument;
constexpr auto Native="org.qindaqt.Keyring1";
constexpr auto Root="/org/freedesktop/secrets";
bool unlocked(const QVariantMap &map) {
    if(map.size()!=6) return false;
    for(const auto &key:{"SettingsAvailable","ScreenLockAvailable","IdleAvailable","ScreenLocked","LockOnScreenLock"})
        if(map.value(key).metaType().id()!=QMetaType::Bool) return false;
    const auto minutes=map.value("LockAfterIdleMinutes");
    return minutes.metaType().id()==QMetaType::Int && minutes.toInt()>=0 && minutes.toInt()<=1440
        && map.value("ScreenLockAvailable").toBool() && !map.value("ScreenLocked").toBool();
}
class SignalReceiver final:public QObject {
    Q_OBJECT
public:
    std::function<void(const QString &,const QDBusObjectPath &,uint,const QByteArray &,const QDBusMessage &)> prompt;
    std::function<void(const QVariantMap &,const QDBusMessage &)> policy;
    std::function<void()> disconnected;
    std::function<void(const QString &,const QVariantMap &,const QDBusMessage &)> receipt;
    std::function<void(const QString &,const QByteArray &,const QDBusObjectPath &,const QDBusMessage &)> secret;
public Q_SLOTS:
    void Disconnected() {disconnected();}
    void PolicyStateReceipt(const QString &nonce,const QVariantMap &state,const QDBusMessage &message) {receipt(nonce,state,message);}
    void PortalSecretResult(const QString &nonce,const QByteArray &bytes,const QDBusObjectPath &path,const QDBusMessage &message) {secret(nonce,bytes,path,message);}
    void PortalPromptResult(const QString &nonce,const QDBusObjectPath &path,uint response,const QByteArray &bytes,const QDBusMessage &message) {prompt(nonce,path,response,bytes,message);}
    void PolicyStateChanged(const QVariantMap &state,const QDBusMessage &message) {policy(state,message);}
};
}
class QtKeyringPortalBroker::Private {
public:
    struct Request {quint64 token=0;QString app,prompt,daemonOwner,policyNonce,secretNonce,portalNonce;SecretPages pages;std::function<void()> next;};
    Private(QtKeyringPortalBroker &object,QDBusConnection connection):q(object),bus(std::move(connection)),
      watcher(Native,bus,QDBusServiceWatcher::WatchForOwnerChange,&q) {
        watcher.addWatchedService("org.freedesktop.secrets");
        receiver.disconnected=[this] {lose();};
        bus.connect(QString(),"/org/freedesktop/DBus/Local","org.freedesktop.DBus.Local","Disconnected",&receiver,SLOT(Disconnected()));
        receiver.prompt=[this](const QString &nonce,const QDBusObjectPath &path,uint response,const QByteArray &wire,const QDBusMessage &message) {
            auto bytes=wire;auto scrub=qScopeGuard([&]{qindaqt::keyring::protocol::wipe(bytes);});
            if(message.service()!=owner || message.signature()!="souay" || !liveOwner()) return;
            std::shared_ptr<Request> found;
            for(const auto &[token,r]:requests) {(void)token;if(r->portalNonce==nonce && r->prompt==path.path()) {found=r;break;}}
            if(!found) return;
            found->portalNonce.clear();
            if(response>2 || (response && !bytes.isEmpty())) {finish(found,{},BrokerError::Failed);return;}
            if(response) {finish(found,{},response==1?BrokerError::Cancelled:BrokerError::Failed);return;}
            accept(found,bytes);
        };
        receiver.receipt=[this](const QString &nonce,const QVariantMap &state,const QDBusMessage &message) {
            if(message.service()!=owner || message.signature()!="sa{sv}" || !liveOwner()) return;
            for(const auto &[token,r]:requests) {
                (void)token;if(r->policyNonce!=nonce) continue;
                r->policyNonce.clear();policyReady=unlocked(state);
                if(!policyReady) {finish(r,{},BrokerError::Unavailable);return;}
                auto next=std::move(r->next);if(next) next();return;
            }
        };
        receiver.secret=[this](const QString &nonce,const QByteArray &wire,const QDBusObjectPath &path,const QDBusMessage &message) {
            // AGENT-GUARD: RPC replies carry no sender proof in Qt. Only a
            // fresh correlated signal from the retained daemon grants bytes.
            auto bytes=wire;auto scrub=qScopeGuard([&]{qindaqt::keyring::protocol::wipe(bytes);});
            const auto found=receipts.find(nonce);if(found==receipts.end()) return;
            const auto r=found->second.lock();if(!r || message.service()!=r->daemonOwner || message.signature()!="sayo" || !liveOwner()) return;
            const auto prompt=path.path();
            if(!active(r)) {
                if(prompt.startsWith(QString(Root)+"/prompt/") && prompt.size()<=256) {
                    auto dismiss=QDBusMessage::createMethodCall(r->daemonOwner,prompt,"org.freedesktop.Secret.Prompt","Dismiss");bus.asyncCall(dismiss,1000);
                }
                receipts.erase(found);return;
            }
            receipts.erase(found);r->portalNonce=r->secretNonce;r->secretNonce.clear();
            if(prompt=="/") {accept(r,bytes);return;}
            if(!bytes.isEmpty() || !prompt.startsWith(QString(Root)+"/prompt/") || prompt.size()>256) {finish(r,{},BrokerError::Failed);return;}
            r->prompt=prompt;
            auto start=QDBusMessage::createMethodCall(owner,prompt,"org.freedesktop.Secret.Prompt","Prompt");start.setArguments({QString()});
            auto *activation=new QDBusPendingCallWatcher(bus.asyncCall(start,2000),&q);
            QObject::connect(activation,&QDBusPendingCallWatcher::finished,&q,[this,r,activation] {
                const auto result=activation->reply();activation->deleteLater();if(active(r) && result.type()!=QDBusMessage::ReplyMessage) {dismiss(r);finish(r,{},BrokerError::Failed);}
            });
        };
        receiver.policy=[this](const QVariantMap &state,const QDBusMessage &message) {
            if(message.service()!=owner || message.signature()!="a{sv}") return;
            policyReady=unlocked(state);
            if(!policyReady || !liveOwner()) lose();
        };
        QObject::connect(&watcher,&QDBusServiceWatcher::serviceOwnerChanged,&q,[this](const QString &,const QString &oldOwner,const QString &) {
            // Queued initial advertisement cannot revoke a freshly admitted
            // same owner. Actual retained-owner loss/reclaim still retires.
            if(!owner.isEmpty() && (oldOwner==owner || !liveOwner())) lose();
        });
    }
    ~Private() {lose(false);}
    bool liveOwner() const {
        if(owner.isEmpty() || !bus.isConnected() || !bus.interface()) return false;
        const auto native=bus.interface()->serviceOwner(Native),secret=bus.interface()->serviceOwner("org.freedesktop.secrets");
        const auto uid=bus.interface()->serviceUid(owner);
        return native.isValid() && secret.isValid() && native.value()==owner && secret.value()==owner && uid.isValid() && uid.value()==geteuid();
    }
    bool active(const std::shared_ptr<Request> &r) const {
        const auto it=requests.find(r->token);return it!=requests.end() && it->second==r;
    }
    void dismiss(const std::shared_ptr<Request> &r) {
        r->next={};r->policyNonce.clear();r->portalNonce.clear();
        if(!r->prompt.isEmpty()) {
            auto message=QDBusMessage::createMethodCall(owner,r->prompt,"org.freedesktop.Secret.Prompt","Dismiss");bus.asyncCall(message,1000);
            r->prompt.clear();
        }
        if(r->pages) r->pages->clear();
        r->pages.reset();
    }
    void lose(bool publish=true) {
        policyReady=false;
        auto previous=std::move(requests);requests.clear();
        for(const auto &[token,r]:previous) {dismiss(r);if(publish) Q_EMIT q.completed(token,{},BrokerError::Unavailable);}
        if(!owner.isEmpty()) {
            bus.disconnect(owner,Root,Native,"PolicyStateChanged",&receiver,SLOT(PolicyStateChanged(QVariantMap,QDBusMessage)));
            bus.disconnect(owner,Root,Native,"PolicyStateReceipt",&receiver,SLOT(PolicyStateReceipt(QString,QVariantMap,QDBusMessage)));
            bus.disconnect(owner,Root,Native,"PortalPromptResult",&receiver,SLOT(PortalPromptResult(QString,QDBusObjectPath,uint,QByteArray,QDBusMessage)));
            bus.disconnect(owner,Root,Native,"PortalSecretResult",&receiver,SLOT(PortalSecretResult(QString,QByteArray,QDBusObjectPath,QDBusMessage)));
        }
        receipts.clear();owner.clear();if(publish) Q_EMIT q.authorityLost();
    }
    bool pin() {
        if(liveOwner()) return true;
        if(!owner.isEmpty() || !requests.empty()) {lose();return false;}
        policyReady=false;
        if(!bus.interface()) return false;
        const auto reply=bus.interface()->serviceOwner(Native);
        if(!reply.isValid()) return false;
        owner=reply.value();
        if(!liveOwner()) {owner.clear();return false;}
        if(!bus.connect(owner,Root,Native,"PolicyStateChanged",&receiver,SLOT(PolicyStateChanged(QVariantMap,QDBusMessage)))
            || !bus.connect(owner,Root,Native,"PolicyStateReceipt",&receiver,SLOT(PolicyStateReceipt(QString,QVariantMap,QDBusMessage)))
            || !bus.connect(owner,Root,Native,"PortalPromptResult",&receiver,SLOT(PortalPromptResult(QString,QDBusObjectPath,uint,QByteArray,QDBusMessage)))
            || !bus.connect(owner,Root,Native,"PortalSecretResult",&receiver,SLOT(PortalSecretResult(QString,QByteArray,QDBusObjectPath,QDBusMessage)))) {lose(false);return false;}
        return true;
    }
    void finish(std::shared_ptr<Request> r,SecretPages pages,BrokerError error) {
        if(!active(r)) {if(pages) pages->clear();return;}
        r->prompt.clear();r->portalNonce.clear();
        requests.erase(r->token);r->next={};r->pages.reset();Q_EMIT q.completed(r->token,std::move(pages),error);
    }
    void policy(const std::shared_ptr<Request> &r,std::function<void()> next) {
        r->policyNonce=QUuid::createUuid().toString(QUuid::WithoutBraces);r->next=std::move(next);
        auto message=QDBusMessage::createMethodCall(owner,Root,Native,"RequestPolicyState");message.setArguments({r->policyNonce});
        sendRequest(r,message);
    }
    void sendRequest(const std::shared_ptr<Request> &r,const QDBusMessage &message) {
        auto *call=new QDBusPendingCallWatcher(bus.asyncCall(message,2000),&q);
        QObject::connect(call,&QDBusPendingCallWatcher::finished,&q,[this,r,call] {
            const auto reply=call->reply();call->deleteLater();
            // Success is only transport acknowledgement. Error may fail closed.
            if(active(r) && reply.type()!=QDBusMessage::ReplyMessage) {dismiss(r);finish(r,{},BrokerError::Unavailable);}
        });
    }
    void accept(const std::shared_ptr<Request> &r,QByteArray &bytes) {
        auto scrub=qScopeGuard([&]{qindaqt::keyring::protocol::wipe(bytes);});
        if(!active(r) || (bytes.size()!=static_cast<qsizetype>(SecretSize) && bytes.size()!=static_cast<qsizetype>(LegacySecretSize)) || !liveOwner()) {finish(r,{},BrokerError::Failed);return;}
        try {
            r->pages=std::make_shared<qindaqt::keyring::SecureBuffer>(static_cast<std::size_t>(bytes.size()));
            std::copy(bytes.cbegin(),bytes.cend(),r->pages->bytes().begin());
            policy(r,[this,r] {auto pages=std::move(r->pages);finish(r,std::move(pages),BrokerError::None);});
        } catch(const std::exception &) {finish(r,{},BrokerError::Failed);}
    }
    void acquire(const std::shared_ptr<Request> &r) {
        if(receipts.size()>=64) {finish(r,{},BrokerError::Unavailable);return;}
        r->daemonOwner=owner;r->secretNonce=QUuid::createUuid().toString(QUuid::WithoutBraces);
        receipts.emplace(r->secretNonce,r);
        auto message=QDBusMessage::createMethodCall(owner,Root,Native,"RequestPortalSecret");message.setArguments({r->app,r->secretNonce});
        sendRequest(r,message);
    }
    QtKeyringPortalBroker &q;QDBusConnection bus;QString owner;bool policyReady=false;
    QDBusServiceWatcher watcher;SignalReceiver receiver;
    std::map<quint64,std::shared_ptr<Request>> requests;
    std::map<QString,std::weak_ptr<Request>> receipts;
};
QtKeyringPortalBroker::QtKeyringPortalBroker(QDBusConnection bus,QObject *parent):SecretBroker(parent),d(std::make_unique<Private>(*this,std::move(bus))) {}
QtKeyringPortalBroker::~QtKeyringPortalBroker()=default;
bool QtKeyringPortalBroker::admitted() const {return d->policyReady && d->liveOwner();}
void QtKeyringPortalBroker::retrieve(quint64 token,const QString &app) {
    if(d->requests.contains(token)) {cancel(token);Q_EMIT completed(token,{},BrokerError::Unavailable);return;}
    if(!token || d->requests.size()>=8 || !validApplicationId(app) || !d->pin()) {Q_EMIT completed(token,{},BrokerError::Unavailable);return;}
    auto request=std::make_shared<Private::Request>();request->token=token;request->app=app;d->requests.emplace(token,request);
    QTimer::singleShot(30000,this,[this,request] {if(d->active(request)) {d->dismiss(request);d->finish(request,{},BrokerError::Cancelled);}d->receipts.erase(request->secretNonce);});
    d->policy(request,[this,request] {d->acquire(request);});
}
void QtKeyringPortalBroker::cancel(quint64 token) {
    const auto found=d->requests.find(token);if(found==d->requests.end()) return;
    auto request=found->second;d->requests.erase(found);d->dismiss(request);
}
}
#include "qt_keyring_broker.moc"
