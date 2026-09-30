// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/secret_portal/secret_portal_adaptor.h>
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <qindaqt/services/secret_portal/legacy_import.h>
#include "fd_writer_p.h"
#include <QDBusConnectionInterface>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QTimer>
#include <map>
#include <algorithm>
#include <QDBusError>
#include <unistd.h>
namespace QindaQt::Services::SecretPortal {
namespace {
class CloseObject final:public QDBusVirtualObject {
public:
    std::function<bool(const QDBusMessage &)> close;
    QString introspect(const QString &) const override {return "<interface name=\"org.freedesktop.impl.portal.Request\"><method name=\"Close\"/></interface>";}
    bool handleMessage(const QDBusMessage &m,const QDBusConnection &bus) override {
        if(m.interface()!=RequestInterface || m.member()!="Close" || !m.signature().isEmpty()) return false;
        const bool allowed=close(m);bus.send(allowed?m.createReply():m.createErrorReply("org.freedesktop.DBus.Error.AccessDenied","Request owner unavailable"));return true;
    }
};
}
class SecretPortalAdaptor::Private {
public:
    struct Request {quint64 token=0;QString owner,handle,app;QDBusMessage call;std::unique_ptr<CloseObject> closer;std::unique_ptr<QindaQt::Services::SecretPortal::Private::FdWriter> writer;};
    Private(SecretPortalAdaptor &object,SecretBroker &source,QDBusConnection connection,int deadline)
      :q(object),broker(source),bus(std::move(connection)),timeout(std::clamp(deadline,1,30000)) {
        rateWindow.start();
        QObject::connect(&broker,&SecretBroker::completed,&q,[this](quint64 token,SecretPages pages,BrokerError error) {
            const auto found=requests.find(token);
            if(found==requests.end()) {if(pages) pages->clear();return;}
            if(error!=BrokerError::None || !pages || (pages->size()!=SecretSize && pages->size()!=LegacySecretSize) || !liveFrontend(found->second->owner) || !broker.admitted()) {
                if(pages) pages->clear();
                finish(token,error==BrokerError::Cancelled?1U:2U);return;
            }
            found->second->writer->start(std::move(pages));
        });
        QObject::connect(&broker,&SecretBroker::authorityLost,&q,[this] {retire();});
    }
    ~Private() {retire();}
    bool liveFrontend(const QString &owner) const {
        if(!bus.isConnected() || !bus.interface() || owner.isEmpty()) return false;
        const auto current=bus.interface()->serviceOwner(FrontendName);
        const auto uid=bus.interface()->serviceUid(owner);
        return current.isValid() && current.value()==owner && uid.isValid() && uid.value()==geteuid();
    }
    void finish(quint64 token,quint32 response) {
        const auto found=requests.find(token);if(found==requests.end()) return;
        auto request=std::move(found->second);requests.erase(found);
        broker.cancel(token);request->writer->cancel();
        bus.unregisterObject(request->handle);
        // Close may be the currently dispatched virtual object. Its QObject
        // teardown is deferred after cancellation has already wiped/closed.
        request->closer.release()->deleteLater();
        bus.send(request->call.createReply({response,QVariantMap{}}));
    }
    void retire() {while(!requests.empty()) finish(requests.begin()->first,2);}
    bool begin(const QDBusMessage &call,const QString &handle,const QString &app,int fd) {
        if(requests.size()>=8) return false;
        for(const auto &[token,r]:requests) {(void)token;if(r->handle==handle) return false;}
        if(rateWindow.elapsed()>=60000) {rates.clear();rateWindow.restart();}
        if((!rates.contains(app) && rates.size()>=128) || rates[app]>=16) return false;
        ++rates[app];const quint64 token=++serial;
        auto r=std::make_unique<Request>();r->token=token;r->owner=call.service();r->handle=handle;r->app=app;r->call=call;
        const auto owner=r->owner;
        r->writer=std::make_unique<QindaQt::Services::SecretPortal::Private::FdWriter>(fd,[this,owner] {return liveFrontend(owner) && broker.admitted();},
            [this,token](bool success) {QTimer::singleShot(0,&q,[this,token,success] {
                const auto it=requests.find(token);if(it==requests.end()) return;
                finish(token,success && liveFrontend(it->second->owner) && broker.admitted()?0U:2U);
            });});
        if(!r->writer->valid()) return false;
        r->closer=std::make_unique<CloseObject>();
        r->closer->close=[this,token,owner](const QDBusMessage &message) {
            if(message.service()!=owner || !liveFrontend(owner)) return false;
            // Close takes effect synchronously, before its object is deleted.
            finish(token,1);return true;
        };
        if(!bus.registerVirtualObject(handle,r->closer.get())) return false;
        requests.emplace(token,std::move(r));
        QTimer::singleShot(timeout,&q,[this,token] {finish(token,1);});
        broker.retrieve(token,app);return true;
    }
    SecretPortalAdaptor &q;SecretBroker &broker;QDBusConnection bus;int timeout;quint64 serial=0;
    QElapsedTimer rateWindow;QMap<QString,int> rates;std::map<quint64,std::unique_ptr<Request>> requests;
};
SecretPortalAdaptor::SecretPortalAdaptor(QObject &host,SecretBroker &broker,QDBusConnection bus,int timeout)
 :QDBusAbstractAdaptor(&host),d(std::make_unique<Private>(*this,broker,std::move(bus),timeout)) {
    d->bus.connect("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","NameOwnerChanged",this,SLOT(ownerChanged(QString,QString,QString)));
}
SecretPortalAdaptor::~SecretPortalAdaptor()=default;
quint32 SecretPortalAdaptor::RetrieveSecret(const QDBusObjectPath &handle,const QString &app,const QDBusUnixFileDescriptor &fd,const QVariantMap &options,const QDBusMessage &call,QVariantMap &results) {
    results.clear();
    if(!d->liveFrontend(call.service())) {call.setDelayedReply(true);d->bus.send(call.createErrorReply("org.freedesktop.DBus.Error.AccessDenied","Secret frontend owner unavailable"));return 2;}
    if(!validRequestHandle(handle.path()) || !validApplicationId(app) || !validOptions(options) || !fd.isValid()) return 2;
    call.setDelayedReply(true);
    if(!d->begin(call,handle.path(),app,fd.fileDescriptor())) d->bus.send(call.createReply({2U,QVariantMap{}}));
    return 2;
}
void SecretPortalAdaptor::ownerChanged(const QString &name,const QString &oldOwner,const QString &) {
    if(name!=FrontendName) return;
    for(const auto &[token,r]:d->requests) {
        (void)token;if(r->owner==oldOwner || !d->liveFrontend(r->owner)) {d->retire();return;}
    }
}
}
