// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/voice_configuration/qt_voice_configuration_transport.h>
#include "native_voice_wire_p.h"
#include "native_voice_codec_p.h"
#include <QtCore/QTimer>
#include <QtCore/QPointer>
#include <utility>
namespace QindaQt::Services::VoiceConfiguration {
namespace {
constexpr auto Driver="org.freedesktop.DBus";
constexpr auto DriverPath="/org/freedesktop/DBus";
constexpr auto VoiceName="org.qindaqt.Voice1";
struct Pending {
    std::unique_ptr<NativeVoiceCall> call;
    QString owner;
    quint64 token=0, generation=0;
};
bool match(NativeVoiceWire &wire, const char *rule) {
    auto request=nativeMethod(QString::fromLatin1(Driver),DriverPath,Driver,"AddMatch");
    if (!request || !dbus_message_append_args(request.get(),DBUS_TYPE_STRING,&rule,DBUS_TYPE_INVALID)) return false;
    const auto reply=wire.call(std::move(request),500);
    return reply && dbus_message_get_type(reply.get())==DBUS_MESSAGE_TYPE_METHOD_RETURN
        && dbus_message_has_signature(reply.get(),"");
}
}
struct QtTransport::Private {
    explicit Private(QString value) : address(std::move(value)) {}
    QString address, owner;
    std::unique_ptr<NativeVoiceWire> wire;
    Pending discovery, snapshot, operation;
    quint64 generation=0, epoch=0;
    bool running=false, scheduled=false;
};
QtTransport::QtTransport(QString address, QObject *parent)
    : Transport(parent),d(std::make_unique<Private>(std::move(address))) {}
QtTransport::~QtTransport() { stop(); }
void QtTransport::start() {
    if (d->running) return;
    d->running=true; ++d->generation; ++d->epoch;
    d->wire=std::make_unique<NativeVoiceWire>();
    if (!d->wire->open(d->address)
        || !match(*d->wire,"type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org.qindaqt.Voice1'")
        || !match(*d->wire,"type='signal',interface='org.qindaqt.VoiceConfiguration1',member='Changed',path='/org/qindaqt/VoiceConfiguration1'")) {
        d->wire.reset(); return;
    }
    d->wire->setProgress([this] { scheduleProcess(); });
    d->wire->setHandler([this](DBusMessage *message) {
        const char *sender=dbus_message_get_sender(message);
        if (!sender) return false;
        if (dbus_message_is_signal(message,Driver,"NameOwnerChanged")
            && QString::fromUtf8(sender)==QString::fromLatin1(Driver)
            && dbus_message_has_path(message,DriverPath) && dbus_message_has_signature(message,"sss")) {
            const char *name=nullptr,*old=nullptr,*owner=nullptr;
            if (dbus_message_get_args(message,nullptr,DBUS_TYPE_STRING,&name,
                DBUS_TYPE_STRING,&old,DBUS_TYPE_STRING,&owner,DBUS_TYPE_INVALID)
                && QString::fromUtf8(name)==QString::fromLatin1(VoiceName)) {
                const QString next=QString::fromUtf8(owner);
                const quint64 epoch=d->epoch;
                // Defer outside the native dispatch frame: a consumer may stop us.
                QTimer::singleShot(0,this,[this,next,epoch] {
                    if (d->running && epoch==d->epoch) { ++d->generation; setOwner(next); }
                });
            }
        } else if (dbus_message_is_signal(message,Interface,"Changed")
            && dbus_message_has_path(message,ObjectPath) && dbus_message_has_signature(message,"t")
            && QString::fromUtf8(sender)==d->owner) {
            const auto owner=d->owner; const auto generation=d->generation;
            QTimer::singleShot(0,this,[this,owner,generation] {
                if (d->running && generation==d->generation && owner==d->owner) Q_EMIT invalidated(owner);
            });
        }
        return false;
    });
    auto request=nativeMethod(QString::fromLatin1(Driver),DriverPath,Driver,"GetNameOwner");
    const char *name=VoiceName;
    if (request && dbus_message_append_args(request.get(),DBUS_TYPE_STRING,&name,DBUS_TYPE_INVALID))
        d->discovery={d->wire->send(std::move(request),3000),{},0,d->generation};
}
void QtTransport::stop() {
    d->running=false; ++d->generation; ++d->epoch;
    d->discovery={}; d->snapshot={}; d->operation={}; d->wire.reset(); setOwner({});
}
void QtTransport::setOwner(const QString &owner) {
    if (owner==d->owner) return;
    d->owner=owner; d->snapshot={}; d->operation={};
    Q_EMIT ownerChanged(owner);
}
void QtTransport::scheduleProcess() {
    if (d->scheduled) return;
    d->scheduled=true;
    QTimer::singleShot(0,this,[this] { d->scheduled=false; process(); });
}
void QtTransport::process() {
    QPointer<QtTransport> alive(this);
    if (!d->running || !d->wire) return;
    if (!d->wire->connected()) { ++d->generation; setOwner({}); return; }
    if (d->discovery.call && d->discovery.call->completed()) {
        auto pending=std::move(d->discovery); d->discovery={};
        auto reply=pending.call->take(); QString owner;
        if (pending.generation==d->generation && pending.call->fromExpectedPeer(reply.get())
            && nativeText(reply.get(),"s",&owner) && owner.startsWith(QLatin1Char(':'))) setOwner(owner);
        if (!alive || !d->running || !d->wire) return;
    }
    for (const bool operation : {false,true}) {
        auto &slot=operation ? d->operation : d->snapshot;
        if (!slot.call || !slot.call->completed()) continue;
        auto pending=std::move(slot); slot={}; auto reply=pending.call->take();
        if (pending.generation!=d->generation || pending.owner!=d->owner) continue;
        const bool authentic=pending.call->fromExpectedPeer(reply.get());
        QVariantMap map; const bool good=authentic && nativeMap(reply.get(),&map);
        const char *error=authentic ? dbus_message_get_error_name(reply.get()) : nullptr;
        const bool unsupported=error && (QString::fromLatin1(error)==QLatin1String(DBUS_ERROR_UNKNOWN_METHOD)
            || QString::fromLatin1(error)==QLatin1String(DBUS_ERROR_UNKNOWN_INTERFACE)
            || QString::fromLatin1(error)==QLatin1String(DBUS_ERROR_UNKNOWN_OBJECT));
        if (operation) Q_EMIT operationReply(pending.owner,pending.token,good,map);
        else Q_EMIT snapshotReply(pending.owner,pending.token,good,unsupported,map);
        if (!alive || !d->running || !d->wire) return;
    }
}
void QtTransport::call(const QString &owner, quint64 token, const QString &method,
                       const QVariantList &arguments, bool operation) {
    if (!d->running || !d->wire || owner.isEmpty() || owner!=d->owner) {
        if (operation) Q_EMIT operationReply(owner,token,false,{});
        else Q_EMIT snapshotReply(owner,token,false,false,{});
        return;
    }
    auto request=nativeMethod(owner,ObjectPath,Interface,method.toLatin1().constData());
    auto &slot=operation ? d->operation : d->snapshot;
    if (!request || !appendArguments(request.get(),arguments)) return;
    slot={d->wire->send(std::move(request),operation ? 20000 : 3000),owner,token,d->generation};
    if (!slot.call) {
        if (operation) Q_EMIT operationReply(owner,token,false,{});
        else Q_EMIT snapshotReply(owner,token,false,false,{});
    }
}
void QtTransport::fetch(const QString &owner, quint64 token) {
    call(owner,token,QStringLiteral("GetSnapshot"),{},false);
}
void QtTransport::submit(const QString &owner, quint64 token, quint64 request,
                         quint64 revision, Operation operation, const QString &key) {
    QVariantList arguments{QVariant::fromValue(request),QVariant::fromValue(revision)};
    if (operation==Operation::SaveElevenLabsKey) arguments.append(key);
    call(owner,token,operation==Operation::SaveElevenLabsKey
        ? QStringLiteral("SaveElevenLabsKey") : QStringLiteral("ReloadCredentials"),arguments,true);
}
}
