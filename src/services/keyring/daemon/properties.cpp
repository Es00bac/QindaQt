// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <QDateTime>
#include <QTimer>
#include <QUuid>
#include <QCoreApplication>
#include <QDBusConnectionInterface>
namespace qindaqt::keyring::service {
QVariantMap SecretService::properties(const QString &path, const QString &iface) const {
    if (path == Root && iface == ServiceInterface) {
        Paths names; for (const auto &id : repository_.names()) names.append(objectPath(collectionPath(id)));
        return {{"Collections",QVariant::fromValue(names)}};
    }
    const auto id = collectionForPath(path);
    if (id.isEmpty()) return {};
    const auto c = repository_.find(id);
    const auto itemId = itemForPath(path,id);
    if (iface == CollectionInterface && itemId.isEmpty()) {
        return {{"Items",QVariant::fromValue(paths(id,repository_.search(id,{})))},
            {"Label",c->label},{"Locked",repository_.locked(id)},
            {"Created",QVariant::fromValue(c->created)},{"Modified",QVariant::fromValue(c->modified)}};
    }
    if (iface == ItemInterface && !itemId.isEmpty()) {
        const auto item = repository_.item(id,itemId);
        return {{"Locked",repository_.locked(id)},{"Label",item ? QString::fromStdString(item->metadata.label) : itemId},
            {"Attributes",QVariant::fromValue(item ? attributes(item->attributes) : StringMap{})},
            {"Created",QVariant::fromValue(item ? item->metadata.created : quint64(0))},
            {"Modified",QVariant::fromValue(item ? item->metadata.modified : quint64(0))}};
    }
    return {};
}
bool SecretService::propertyMethod(const QDBusMessage &m) {
    const auto args = m.arguments();
    if ((m.member() == "Get" && m.signature() == "ss") || (m.member() == "GetAll" && m.signature() == "s")) {
        const auto props = properties(m.path(),args[0].toString());
        if (props.isEmpty()) { error(m,"org.freedesktop.DBus.Error.UnknownInterface"); return true; }
        if (m.member() == "GetAll") reply(m,{props});
        else if (props.contains(args[1].toString())) reply(m,{QVariant::fromValue(QDBusVariant(props.value(args[1].toString())))});
        else error(m,"org.freedesktop.DBus.Error.UnknownProperty");
        return true;
    }
    if (m.member() == "Set" && m.signature() == "ssv") {
        const auto iface = args[0].toString(), name = args[1].toString(), id = collectionForPath(m.path());
        if (id.isEmpty()) throw std::runtime_error("Unknown collection");
        if (repository_.locked(id)) { error(m,"org.freedesktop.Secret.Error.IsLocked"); return true; }
        const auto value = argument<QDBusVariant>(args[2]).variant();
        const auto itemId = itemForPath(m.path(),id);
        if (iface == CollectionInterface && name == "Label" && itemId.isEmpty()
            && value.metaType() == QMetaType::fromType<QString>()) repository_.setLabel(id,value.toString());
        else if (iface == ItemInterface && !itemId.isEmpty() && (name == "Label" || name == "Attributes")) {
            const auto old = repository_.item(id,itemId);
            if (!old) throw std::runtime_error("Unknown item");
            Item item; item.id = old->id; item.metadata = old->metadata; item.attributes = old->attributes;
            item.secret = copySecret(old->secret);
            if (name == "Label") {
                if (value.metaType() != QMetaType::fromType<QString>() || value.toString().size() > 1024 || value.toString().toUtf8().size() > 1024)
                    throw std::runtime_error("Invalid label");
                item.metadata.label = value.toString().toStdString();
            } else item.attributes = attributes(argument<StringMap>(value));
            item.metadata.modified = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
            if (!repository_.put(id,std::move(item))) throw std::runtime_error("Persistence unavailable");
            signal(collectionPath(id),CollectionInterface,"ItemChanged",{variantPath(m.path())});
        } else { error(m,"org.freedesktop.DBus.Error.PropertyReadOnly"); return true; }
        changed(m.path(),iface,{{name,value}}); reply(m); return true;
    }
    error(m,"org.freedesktop.DBus.Error.UnknownMethod"); return true;
}
bool SecretService::nativeMethod(const QDBusMessage &m) {
    if (metadataMethod(m) || portalMethod(m) || nativeItemMethod(m)) return true;
    if(m.member()=="RequestPolicyState") {
        const auto nonce=m.arguments().value(0).toString();
        const auto parsed=QUuid::fromString(nonce);
        if(m.signature()!="s" || parsed.isNull() || parsed.toString(QUuid::WithoutBraces)!=nonce) {
            error(m,"org.freedesktop.DBus.Error.InvalidArgs");return true;
        }
        // AGENT-CONTRACT: UI and portal consume actual sender/nonce receipts.
        // Existing GetPolicyState remains informational/wire-compatible.
        const QVariantMap state=lockPolicy_?lockPolicy_->status():QVariantMap{{"SettingsAvailable",false},{"ScreenLockAvailable",false},{"IdleAvailable",false},{"ScreenLocked",true},{"LockOnScreenLock",false},{"LockAfterIdleMinutes",0}};
        auto receipt=QDBusMessage::createTargetedSignal(m.service(),Root,NativeInterface,"PolicyStateReceipt");receipt.setArguments({nonce,state});bus_.send(receipt);reply(m);return true;
    }
    if (m.member()=="GetPolicyState" && m.signature().isEmpty()) {
        const QVariantMap state=lockPolicy_?lockPolicy_->status():QVariantMap{{"SettingsAvailable",false},{"ScreenLockAvailable",false},{"IdleAvailable",false},{"ScreenLocked",true},{"LockOnScreenLock",false},{"LockAfterIdleMinutes",0}};
        reply(m,{state});return true;
    }
    if (m.member() == "ListCollections" && m.signature().isEmpty()) {
        const auto result=collectionMetadata();
        reply(m,{result}); return true;
    }
    if (m.member() == "AttachSessionWithDisplay" && m.signature() == "s") {
        const bool accepted = (sessionOwner_.isEmpty() || sessionOwner_ == m.service())
            && promptsProvider_.bindSessionDisplay(m.service(),m.arguments()[0].toString());
        if (accepted) sessionOwner_ = m.service();
        reply(m,{accepted}); return true;
    }
    if (m.member() == "AttachSession" && m.signature().isEmpty()) {
        const bool accepted = sessionOwner_.isEmpty() || sessionOwner_ == m.service();
        if (accepted) sessionOwner_ = m.service();
        reply(m,{accepted}); return true;
    }
    if (m.member() == "Shutdown" && m.signature().isEmpty()) {
        reply(m); QCoreApplication::quit(); return true;
    }
    if (m.member() == "ChangePassword" && m.signature() == "o(oayays)(oayays)") {
        if (pendingRekeys_ >= 4) throw std::runtime_error("Request limit");
        const auto id = collectionForPath(argument<QDBusObjectPath>(m.arguments()[0]).path());
        if (id.isEmpty() || id == "session") throw std::runtime_error("Invalid collection");
        auto oldWire = argument<WireSecret>(m.arguments()[1]), newWire = argument<WireSecret>(m.arguments()[2]);
        auto oldPassword = std::make_shared<SecureBuffer>(session(oldWire.session.path(),m.service()).crypto.decode(oldWire,4096));
        auto newPassword = std::make_shared<SecureBuffer>(session(newWire.session.path(),m.service()).crypto.decode(newWire,4096));
        if (oldPassword->size() == 0 || oldPassword->size() > 4096 || newPassword->size() == 0 || newPassword->size() > 4096)
            throw std::runtime_error("Invalid password");
        ++pendingRekeys_;
        const auto owner = m.service();
        const auto oldSession = oldWire.session.path(), newSession = newWire.session.path();
        const bool wasLocked = repository_.locked(id);
        auto acknowledgement = m.createReply();
        QTimer::singleShot(550,this,[this,id,owner,oldSession,newSession,wasLocked,oldPassword,newPassword,acknowledgement]() mutable {
            const auto alive = bus_.interface()->isServiceRegistered(owner);
            if (!alive.isValid() || !alive.value() || !sessions_.contains(oldSession) || !sessions_.contains(newSession) || !repository_.unlock(id,oldPassword->bytes())) {
                --pendingRekeys_; acknowledgement.setArguments({false}); bus_.send(acknowledgement); return;
            }
            oldPassword->clear();
            QTimer::singleShot(550,this,[this,id,owner,oldSession,newSession,wasLocked,newPassword,acknowledgement]() mutable {
                const auto connected = bus_.interface()->isServiceRegistered(owner);
                const bool authorized = connected.isValid() && connected.value() && sessions_.contains(oldSession) && sessions_.contains(newSession);
                if (!authorized && wasLocked) repository_.lock(id);
                const bool saved = authorized && repository_.rekey(id,newPassword->bytes());
                --pendingRekeys_; newPassword->clear(); notifyCollectionState(id);
                acknowledgement.setArguments({saved}); bus_.send(acknowledgement);
            });
        });
        return true;
    }
    error(m,"org.freedesktop.DBus.Error.UnknownMethod"); return true;
}
}
