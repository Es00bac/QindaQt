// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <QDBusConnectionInterface>
#include <QFile>
#include <QTimer>

#include <QUuid>
#include <QCoreApplication>
#include <unistd.h>

namespace qindaqt::keyring::service {
SecretService::SecretService(CollectionRepository &repository, PromptProvider &provider,
                             QDBusConnection bus, QObject *parent)
    : QDBusVirtualObject(parent), repository_(repository), promptsProvider_(provider), bus_(std::move(bus)) {
    registerWireTypes();
    bus_.connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                 "NameOwnerChanged", this, SLOT(ownerLost(QString,QString,QString)));
}
QString SecretService::collectionPath(const QString &id) const {
    return QString(Root) + "/collection/" + (id == "login" || id == "session" ? id : "c" + id.toUtf8().toHex());
}
QString SecretService::itemPath(const QString &id, const QString &item) const {
    return collectionPath(id) + "/i" + item.toUtf8().toHex();
}
QString SecretService::collectionForPath(const QString &path) const {
    for (const auto &id : repository_.names()) {
        const auto p = collectionPath(id);
        if (path == p || path.startsWith(p + '/')) return id;
    }
    if (path.startsWith(QString(Root) + "/aliases/")) {
        const auto alias = path.mid(QString(Root).size() + 9);
        return repository_.alias(alias);
    }
    return {};
}
QString SecretService::itemForPath(const QString &path, const QString &id) const {
    if (path == collectionPath(id)) return {};
    for (const auto &item : repository_.search(id, {}).ids)
        if (path == itemPath(id, QString::fromStdString(item))) return QString::fromStdString(item);
    return {};
}
Paths SecretService::paths(const QString &id, const SearchResult &result) const {
    Paths paths;
    for (const auto &item : result.ids) paths.append(objectPath(itemPath(id, QString::fromStdString(item))));
    return paths;
}
bool SecretService::exists(const QString &path) const {
    if (path == Root || sessions_.contains(path) || prompts_.contains(path)) return true;
    const auto id = collectionForPath(path);
    if (id.isEmpty()) return false;
    return path == collectionPath(id) || (path == QString(Root) + "/aliases/" + path.section('/',-1)
        && repository_.alias(path.section('/',-1)) == id) || !itemForPath(path,id).isEmpty();
}
QString SecretService::introspect(const QString &path) const {
    if (!exists(path)) return {};
    QFile file(":/keyring/api.xml");
    if (!file.open(QIODevice::ReadOnly)) return {};

    QString result;
    const auto id = collectionForPath(path);
    const QString selected = path == Root ? QString(ServiceInterface)
        : path.startsWith(QString(Root) + "/session/") ? "org.freedesktop.Secret.Session"
        : path.startsWith(QString(Root) + "/prompt/") ? "org.freedesktop.Secret.Prompt"
        : id.isEmpty() ? QString() : itemForPath(path,id).isEmpty() ? QString(CollectionInterface) : QString(ItemInterface);
    // Generated XML contains each cohesive interface as one text fragment.
    const auto xml = QString::fromUtf8(file.seek(0) ? file.readAll() : QByteArray{});
    for (const auto &iface : {selected, QString("org.freedesktop.DBus.Properties"),
                              QString("org.freedesktop.DBus.Introspectable"),
                              path == Root ? QString(NativeInterface) : QString()}) {
        if (iface.isEmpty()) continue;
        const auto begin = xml.indexOf("<interface name=\"" + iface + "\"");
        const auto end = xml.indexOf("</interface>", begin);
        if (begin >= 0 && end >= begin) result += xml.mid(begin, end + 12 - begin);
    }
    return result;
}
bool SecretService::nativeDisclosureAllowed() const {
    if(!lockPolicy_) return false;
    const auto state=lockPolicy_->status();
    return state.value("ScreenLockAvailable").toBool() && !state.value("ScreenLocked").toBool();
}
void SecretService::observeLockPolicy(KeyringLockPolicy *policy) {
    lockPolicy_=policy;
    if(policy) connect(policy,&KeyringLockPolicy::changed,this,[this]{
        if(!nativeDisclosureAllowed()) {
            if (!sessionOwner_.isEmpty()) cancelRelatedUnlock();
            QStringList retired;
            for(const auto &[path,prompt]:prompts_) if(prompt.action=="reveal" || prompt.action.startsWith("portal-")) retired.append(path);
            for(const auto &path:retired) finishPrompt(path,true);
        }
        if(lockPolicy_) signal(Root,NativeInterface,"PolicyStateChanged",{lockPolicy_->status()});
    });
}
void SecretService::cancelRelatedUnlock() {
    ++relatedGeneration_;
    relatedPassword_.clear();
    relatedCollections_.clear();
    auto completed = std::move(relatedCompleted_);
    if (completed) QTimer::singleShot(0,this,std::move(completed));
}
void SecretService::unlockRelated(const QString &authenticatedId, SecureBuffer password,
                                  std::function<void()> completed) {
    cancelRelatedUnlock();
    relatedPassword_ = std::move(password);
    relatedCompleted_ = std::move(completed);
    for (const auto &id : repository_.names()) {
        const auto *collection = repository_.find(id);
        if (id != authenticatedId && collection && collection->storage && repository_.locked(id))
            relatedCollections_.append(id);
    }
    continueRelatedUnlock(relatedGeneration_);
}
void SecretService::continueRelatedUnlock(quint64 generation) {
    if (generation != relatedGeneration_) return;
    if (relatedCollections_.isEmpty() || (lockPolicy_ && !sessionOwner_.isEmpty() && !nativeDisclosureAllowed())) {
        cancelRelatedUnlock();
        return;
    }
    QTimer::singleShot(550,this,[this,generation] {
        if (generation != relatedGeneration_) return;
        if (lockPolicy_ && !sessionOwner_.isEmpty() && !nativeDisclosureAllowed()) {
            cancelRelatedUnlock(); return;
        }
        const auto id = relatedCollections_.takeFirst();
        try {
            if (repository_.locked(id) && repository_.unlock(id,relatedPassword_.bytes()))
                notifyCollectionState(id);
        } catch (const std::exception &) { cancelRelatedUnlock(); return; }
        continueRelatedUnlock(generation);
    });
}
void SecretService::notifyCollectionState(const QString &id) {
    if(lockPolicy_) lockPolicy_->enforce();
    if (!repository_.find(id)) return;
    changed(collectionPath(id),CollectionInterface,{{"Locked",repository_.locked(id)}});
    for (const auto &item : repository_.search(id,{}).ids)
        changed(itemPath(id,QString::fromStdString(item)),ItemInterface,{{"Locked",repository_.locked(id)}});
    signal(Root,NativeInterface,"CollectionStateChanged",{variantPath(collectionPath(id)),repository_.locked(id),repository_.search(id,{}).authenticated});
}
void SecretService::reply(const QDBusMessage &message, const QVariantList &arguments) {
    bus_.send(message.createReply(arguments));
}
void SecretService::error(const QDBusMessage &m, const QString &name) {
    bus_.send(m.createErrorReply(name, "Keyring request could not be completed"));
}
void SecretService::signal(const QString &path, const QString &iface, const QString &name, const QVariantList &args) {
    auto message = QDBusMessage::createSignal(path, iface, name); message.setArguments(args); bus_.send(message);
}
void SecretService::changed(const QString &path, const QString &iface, const QVariantMap &props) {
    signal(path, "org.freedesktop.DBus.Properties", "PropertiesChanged", {iface, props, QStringList{}});
}
Session &SecretService::session(const QString &path, const QString &owner) {
    const auto i = sessions_.find(path);
    if (i == sessions_.end() || i->second->owner != owner) throw std::runtime_error("Session unavailable");
    return *i->second;
}
void SecretService::ownerLost(const QString &name, const QString &, const QString &newOwner) {
    if(name=="org.freedesktop.impl.portal.desktop.qindaqt") {
        QStringList retired;
        for(const auto &[path,prompt]:prompts_) if(prompt.action.startsWith("portal-")) retired.append(path);
        for(const auto &path:retired) finishPrompt(path,true);
    }
    if (name == sessionOwner_ && newOwner.isEmpty()) { QCoreApplication::quit(); return; }
    if (!name.startsWith(':') || !newOwner.isEmpty()) return;
    for (auto i = sessions_.begin(); i != sessions_.end();) {
        if (i->second->owner == name) i = sessions_.erase(i); else ++i;
    }
    for (auto i = prompts_.begin(); i != prompts_.end();) {
        if (i->second.owner == name) {
            const auto path = i->first; ++i; finishPrompt(path,true);
        }
        else ++i;
    }
}
bool SecretService::handleMessage(const QDBusMessage &m, const QDBusConnection &) {
    if (m.type() != QDBusMessage::MethodCallMessage) return false;
    if (m.service().isEmpty() || !bus_.interface()) { error(m, "org.freedesktop.DBus.Error.AccessDenied"); return true; }
    const auto uid = bus_.interface()->serviceUid(m.service());
    if (!uid.isValid() || uid.value() != geteuid()) { error(m, "org.freedesktop.DBus.Error.AccessDenied"); return true; }
    try {
        // Authenticated control rekey may temporarily hold decrypted pages.
        // Every wire disclosure must re-evaluate resident policy first.
        if(lockPolicy_) lockPolicy_->enforce();
        if (!exists(m.path())) { error(m,"org.freedesktop.Secret.Error.NoSuchObject"); return true; }
        if (m.interface() == "org.freedesktop.DBus.Introspectable"
            && m.member() == "Introspect" && m.signature().isEmpty()) {
            // AGENT-CONTRACT: QDBusVirtualObject supplies the interface XML but
            // still routes the standard wire call here. Clients need the full
            // node document to discover both Secret Service and Keyring1.
            reply(m, {QStringLiteral("<node>") + introspect(m.path()) + QStringLiteral("</node>")});
            return true;
        }
        if (m.interface() == "org.freedesktop.DBus.Properties") return propertyMethod(m);
        if (m.interface() == "org.freedesktop.Secret.Prompt") return promptMethod(m);
        if (m.interface() == "org.freedesktop.Secret.Session" && m.member() == "Close" && m.signature().isEmpty()) {
            session(m.path(),m.service()); sessions_.erase(m.path()); reply(m); return true;
        }
        if (m.path() == Root && m.interface() == ServiceInterface) return serviceMethod(m);
        if (m.path() == Root && m.interface() == NativeInterface) return nativeMethod(m);
        const auto id = collectionForPath(m.path());
        if (!id.isEmpty()) {
            const auto item = itemForPath(m.path(),id);
            if (m.interface() == CollectionInterface && item.isEmpty()) return collectionMethod(m,id);
            if (m.interface() == ItemInterface && !item.isEmpty()) return itemMethod(m,id,item);
        }
        error(m, "org.freedesktop.DBus.Error.UnknownMethod");
    } catch (const PersistenceError &) {
        error(m, "org.freedesktop.DBus.Error.Failed");
        QCoreApplication::exit(1); // Reload durable catalog before accepting more mutations.
    } catch (const std::exception &) { error(m, "org.freedesktop.DBus.Error.Failed"); }
    return true;
}
}
