// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring_client/qt_keyring_gateway.h>
#include "keyring_reply_validation.h"
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QTimer>
#include <cstring>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
namespace QindaQt::Services::KeyringClient {
namespace p=qindaqt::keyring::protocol;
namespace {
constexpr auto Service="org.freedesktop.secrets";
constexpr auto Root="/org/freedesktop/secrets";
constexpr auto Native="org.qindaqt.Keyring1";
constexpr auto Secret="org.freedesktop.Secret.Service";
constexpr auto PromptInterface="org.freedesktop.Secret.Prompt";
QVariant path(const QString &v) { return QVariant::fromValue(QDBusObjectPath(v)); }
}
class QtKeyringGateway::Private {
public:
    Private(QtKeyringGateway &parentGateway,QDBusConnection b):q(parentGateway),bus(std::move(b)),
        watcher(Service,bus,QDBusServiceWatcher::WatchForOwnerChange,&parentGateway) {
        p::registerWireTypes();
        watcher.addWatchedService(Native);
        struct rlimit core{0,0};
        protectedProcess=setrlimit(RLIMIT_CORE,&core)==0 && prctl(PR_SET_DUMPABLE,0)==0;
        timeout.setSingleShot(true);
        QObject::connect(&timeout,&QTimer::timeout,&q,[this]{ fail("Keyring did not confirm the request"); });
        QObject::connect(&watcher,&QDBusServiceWatcher::serviceOwnerChanged,&q,[this](const QString &,const QString &,const QString &){ reset(); probe(); });
        bus.connect(QString(),"/org/freedesktop/DBus/Local","org.freedesktop.DBus.Local","Disconnected",&q,SLOT(disconnected()));
        QTimer::singleShot(0,&q,[this]{ probe(); });
    }
    using Reply=std::function<void(const QDBusMessage &)>;
    void call(const QString &destination,const QString &object,const QString &interface,
              const QString &member,const QVariantList &arguments,Reply reply) {
        const auto serial=generation;
        auto message=QDBusMessage::createMethodCall(destination,object,interface,member);
        message.setArguments(arguments);
        auto *pending=new QDBusPendingCallWatcher(bus.asyncCall(message,4000),&q);
        QObject::connect(pending,&QDBusPendingCallWatcher::finished,&q,[this,serial,reply=std::move(reply)](QDBusPendingCallWatcher *value){
            const auto response=value->reply();value->deleteLater();
            if(serial!=generation) return;
            if(response.type()!=QDBusMessage::ReplyMessage) { if(token) fail("Keyring did not confirm the request");return; }
            try { reply(response); } catch(const std::exception &) { fail("Invalid keyring response"); }
        });
    }
    void probe() {
        if(!protectedProcess || !bus.isConnected()) return;
        call("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","GetNameOwner",{QString(Service)},[this](const QDBusMessage &m){
            if(m.signature()!="s" || m.arguments().size()!=1) return;
            const auto candidate=m.arguments()[0].toString();
            if(!candidate.startsWith(':') || candidate.size()>128) return;
            call("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","GetConnectionUnixUser",{candidate},[this,candidate](const QDBusMessage &r){
                if(r.signature()!="u" || r.arguments().size()!=1 || r.arguments()[0].toUInt()!=geteuid()) return;
                // Unique bus names cannot be reused. Watcher was installed first;
                // every replacement invalidates this probe's generation.
                call("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","GetNameOwner",{QString(Native)},[this,candidate](const QDBusMessage &nativeOwner){
                    if(nativeOwner.signature()!="s" || nativeOwner.arguments().size()!=1 || nativeOwner.arguments()[0].toString()!=candidate) return;
                    owner=candidate;
                    bus.connect(owner,Root,Native,"CollectionStateChanged",&q,SLOT(collectionStateChanged(QDBusObjectPath,bool,bool,QDBusMessage)));
                    emit q.authorityChanged();
                });
            });
        });
    }
    void closeSession() {
        if(!session.isEmpty() && !owner.isEmpty()) {
            auto message=QDBusMessage::createMethodCall(owner,session,"org.freedesktop.Secret.Session","Close");
            bus.send(message);
        }
        session.clear();
    }
    void unwatchPrompt() {
        if(!prompt.isEmpty()) bus.disconnect(owner,prompt,PromptInterface,"Completed",&q,SLOT(promptCompleted(bool,QDBusVariant,QDBusMessage)));
        prompt.clear();
    }
    void cancel() {
        const bool pending=token || !prompt.isEmpty() || !session.isEmpty();
        if(!prompt.isEmpty() && !owner.isEmpty()) {
            auto message=QDBusMessage::createMethodCall(owner,prompt,PromptInterface,"Dismiss");bus.send(message);
        }
        unwatchPrompt();closeSession();timeout.stop();token=0;if(pending) ++generation;
    }
    void reset() { cancel();++generation;
        if(!owner.isEmpty()) bus.disconnect(owner,Root,Native,"CollectionStateChanged",&q,SLOT(collectionStateChanged(QDBusObjectPath,bool,bool,QDBusMessage)));
        owner.clear();emit q.authorityChanged(); }
    void fail(const QString &message) {
        if(!token) return;
        const auto id=token;cancel();emit q.actionFinished(id,false,message);
    }
    void done(bool confirmed,const QString &message={}) {
        const auto id=token;unwatchPrompt();closeSession();timeout.stop();token=0;++generation;
        emit q.actionFinished(id,confirmed,message);
    }
    void watchPrompt(const QString &value) {
        if(!validObjectPath(value,"prompt")) { fail("Keyring requires an authenticated prompt");return; }
        prompt=value;
        if(!bus.connect(owner,prompt,PromptInterface,"Completed",&q,SLOT(promptCompleted(bool,QDBusVariant,QDBusMessage)))) { fail("Prompt unavailable");return; }
        call(owner,prompt,PromptInterface,"Prompt",{QString()},[](const QDBusMessage &){});
    }
    void begin(quint64 id,Request request,const QString &object,const QString &label) {
        if(!id) return;
        if(token || owner.isEmpty()) { QTimer::singleShot(0,&q,[this,id]{emit q.actionFinished(id,false,"Keyring unavailable or busy");});return; }
        if(request!=Request::Collections && request!=Request::Create && !validObjectPath(object,"collection")) {
            QTimer::singleShot(0,&q,[this,id]{emit q.actionFinished(id,false,"Invalid keyring object");});return;
        }
        if(label.contains(QChar(0)) || label.toUtf8().size()>1024 || (request==Request::Create && label.trimmed().isEmpty())) {
            QTimer::singleShot(0,&q,[this,id]{emit q.actionFinished(id,false,"Invalid collection label");});return;
        }
        token=id;kind=request;timeout.start(45000);
        if(kind==Request::Collections || kind==Request::Items) {
            call(owner,Root,Native,kind==Request::Collections?"ListCollections":"ListItems",
                 kind==Request::Collections?QVariantList{}:QVariantList{path(object)},[this](const QDBusMessage &m){
                if(m.arguments().size()!=1 || m.signature()!=(kind==Request::Collections?"a{sv}":"aa{sv}")) throw std::runtime_error("Metadata");
                const auto rows=kind==Request::Collections?validateCollections(p::argument<QVariantMap>(m.arguments()[0])):validateItems(p::argument<p::MetadataRows>(m.arguments()[0]));
                const auto completedToken=token;token=0;timeout.stop();++generation;emit q.rowsReady(completedToken,rows);
            });return;
        }
        if(kind==Request::Reveal) {
            call(owner,Root,Secret,"OpenSession",{QString("plain"),QVariant::fromValue(QDBusVariant(QString()))},[this,object](const QDBusMessage &m){
                if(m.signature()!="vo" || m.arguments().size()!=2) throw std::runtime_error("Session");
                const auto output=p::argument<QDBusVariant>(m.arguments()[0]).variant();
                if(output.metaType()!=QMetaType::fromType<QString>() || !output.toString().isEmpty()) throw std::runtime_error("Session");
                session=p::argument<QDBusObjectPath>(m.arguments()[1]).path();
                if(!validObjectPath(session,"session")) throw std::runtime_error("Session");
                nativePrompt("ReadSecretWithPrompt",{path(object),path(session)});
            });return;
        }
        if(kind==Request::ChangePassword || kind==Request::Delete) {
            nativePrompt(kind==Request::Delete?"DeleteItemWithPrompt":"ChangePasswordWithPrompt",{path(object)});return;
        }
        if(kind==Request::Create) {
            const QVariantMap properties{{"org.freedesktop.Secret.Collection.Label",label}};
            call(owner,Root,Secret,"CreateCollection",{properties,QString()},[this](const QDBusMessage &m){
                if(m.signature()!="oo" || m.arguments().size()!=2 || p::argument<QDBusObjectPath>(m.arguments()[0]).path()!="/") throw std::runtime_error("Prompt required");
                watchPrompt(p::argument<QDBusObjectPath>(m.arguments()[1]).path());
            });return;
        }
        call(owner,Root,Secret,kind==Request::Lock?"Lock":"Unlock",{QVariant::fromValue(p::Paths{QDBusObjectPath(object)})},[this,object](const QDBusMessage &m){
            if(m.signature()!="aoo" || m.arguments().size()!=2) throw std::runtime_error("Lock");
            const auto completed=p::argument<p::Paths>(m.arguments()[0]);
            const auto value=p::argument<QDBusObjectPath>(m.arguments()[1]).path();
            if(value=="/") done(completed.contains(QDBusObjectPath(object)),"Keyring state confirmed");
            else watchPrompt(value);
        });
    }
    void nativePrompt(const QString &member,const QVariantList &args) {
        call(owner,Root,Native,member,args,[this](const QDBusMessage &m){
            if(m.signature()!="o" || m.arguments().size()!=1) throw std::runtime_error("Prompt");
            watchPrompt(p::argument<QDBusObjectPath>(m.arguments()[0]).path());
        });
    }
    void completed(bool dismissed,const QDBusVariant &result,const QDBusMessage &message) {
        if(!token || message.service()!=owner || message.path()!=prompt) return;
        if(dismissed) { done(false,"Keyring did not confirm the change");return; }
        if(kind!=Request::Reveal) {
            // A durable daemon acknowledgement is required. This never treats
            // prompt cancellation, transport loss or uncertain save as success.
            const auto value=result.variant();
            bool confirmed=false;
            if(kind==Request::Create) confirmed=validObjectPath(p::argument<QDBusObjectPath>(value).path(),"collection");
            else {
                const auto objects=p::argument<p::Paths>(value);
                confirmed=objects.size()==1 && validObjectPath(objects.first().path(),"collection");
            }
            done(confirmed,confirmed?"Keyring confirmed the change":"Invalid keyring acknowledgement");return;
        }
        try {
            auto wire=p::argument<p::WireSecret>(result.variant());
            if(wire.session.path()!=session || !wire.parameters.isEmpty() || wire.value.size()>1024*1024 || wire.contentType.toUtf8().size()>128 || wire.contentType.contains(QChar(0)))
                throw std::runtime_error("Secret");
            auto bytes=std::make_shared<qindaqt::keyring::SecureBuffer>(static_cast<std::size_t>(wire.value.size()));
            if(!wire.value.isEmpty()) std::memcpy(bytes->bytes().data(),wire.value.constData(),bytes->size());
            p::wipe(wire.value);
            const auto id=token;unwatchPrompt();closeSession();timeout.stop();token=0;++generation;
            emit q.secretReady(id,std::move(bytes),wire.contentType);
        } catch(const std::exception &) { fail("Secret unavailable"); }
    }
    QtKeyringGateway &q;
    QDBusConnection bus;
    QDBusServiceWatcher watcher;
    QTimer timeout;
    QString owner,prompt,session;
    quint64 token=0,generation=1;
    Request kind=Request::Collections;
    bool protectedProcess=false;
};
QtKeyringGateway::QtKeyringGateway(QDBusConnection bus,QObject *parent):KeyringGateway(parent),d(std::make_unique<Private>(*this,std::move(bus))) {}
QtKeyringGateway::~QtKeyringGateway(){d->cancel();}
bool QtKeyringGateway::available() const {return !d->owner.isEmpty();}
void QtKeyringGateway::request(quint64 token,Request kind,const QString &path,const QString &label){d->begin(token,kind,path,label);}
void QtKeyringGateway::cancel(){d->cancel();}
void QtKeyringGateway::promptCompleted(bool dismissed,const QDBusVariant &value,const QDBusMessage &message){d->completed(dismissed,value,message);}
void QtKeyringGateway::disconnected(){d->reset();}
void QtKeyringGateway::collectionStateChanged(const QDBusObjectPath &object,bool locked,bool authenticated,const QDBusMessage &message){
    if(message.service()!=d->owner || !validObjectPath(object.path(),"collection")) return;
    if(locked || !authenticated) emit secretsInvalidated();
    emit metadataChanged();
}
}
