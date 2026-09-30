// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <QDateTime>
#include <QUuid>
#include <QDBusConnectionInterface>
#include <QFileInfo>
namespace qindaqt::keyring::service {
bool SecretService::collectionMethod(const QDBusMessage &m, const QString &id) {
    const auto args = m.arguments();
    if (m.member() == "SearchItems" && m.signature() == "a{ss}") {
        reply(m,{QVariant::fromValue(paths(id,repository_.search(id,attributes(argument<StringMap>(args[0])))))}); return true;
    }
    if (repository_.locked(id)) { error(m,"org.freedesktop.Secret.Error.IsLocked"); return true; }
    if (m.member() == "Delete" && m.signature().isEmpty()) {
        repository_.remove(id);
        signal(Root,ServiceInterface,"CollectionDeleted",{variantPath(collectionPath(id))});
        changed(Root,ServiceInterface,properties(Root,ServiceInterface));
        reply(m,{variantPath()}); return true;
    }
    if (m.member() == "CreateItem" && m.signature() == "a{sv}(oayays)b") {
        auto properties = argument<QVariantMap>(args[0]);
        if (properties.size() > 2 || !properties.contains(QString(ItemInterface) + ".Attributes")
            || !properties.contains(QString(ItemInterface) + ".Label")) throw std::runtime_error("Invalid item");
        auto wire = argument<WireSecret>(args[1]);
        auto &s = session(wire.session.path(),m.service());
        Item item;
        item.attributes = attributes(argument<StringMap>(properties.value(QString(ItemInterface) + ".Attributes")));
        const auto label = properties.value(QString(ItemInterface) + ".Label");
        if (label.metaType() != QMetaType::fromType<QString>() || label.toString().size() > 1024)
            throw std::runtime_error("Invalid item");
        item.metadata.label = label.toString().toStdString();
        if (item.metadata.label.size() > 1024) throw std::runtime_error("Invalid item");
        item.metadata.contentType = wire.contentType.toStdString();
        // Best-effort authenticated process hint, never an executable attestation.
        // Protected/disappeared callers retain their captured unique bus identity.
        const auto pid = bus_.interface()->servicePid(m.service());
        const auto executable = pid.isValid() ? QFileInfo(QString("/proc/%1/exe").arg(pid.value())).symLinkTarget() : QString();
        const auto creator = QFileInfo(executable).fileName();
        item.metadata.creator = (creator.isEmpty() || creator.toUtf8().size() > 1024 ? m.service() : creator).toStdString();
        item.metadata.created = item.metadata.modified = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
        if (args[2].toBool()) {
            for (const auto &candidate : repository_.search(id,item.attributes).ids) {
                const auto found = repository_.item(id,QString::fromStdString(candidate));
                if (found && found->attributes == item.attributes) { item.id = candidate; item.metadata.created = found->metadata.created; item.metadata.creator = found->metadata.creator; break; }
            }
        }
        const bool replaced = !item.id.empty();
        if (item.id.empty()) item.id = ("i" + QUuid::createUuid().toString(QUuid::Id128)).toStdString();
        item.secret = s.crypto.decode(wire);
        const auto itemId = QString::fromStdString(item.id);
        if (!repository_.put(id,std::move(item))) throw std::runtime_error("Item persistence unavailable");
        const auto path = itemPath(id,itemId);
        signal(collectionPath(id),CollectionInterface,replaced ? "ItemChanged" : "ItemCreated",{variantPath(path)});
        changed(collectionPath(id),CollectionInterface,{{"Items",QVariant::fromValue(paths(id,repository_.search(id,{})))}});
        reply(m,{variantPath(path),variantPath()}); return true;
    }
    error(m,"org.freedesktop.DBus.Error.UnknownMethod"); return true;
}
bool SecretService::itemMethod(const QDBusMessage &m, const QString &id, const QString &itemId) {
    if (repository_.locked(id)) { error(m,"org.freedesktop.Secret.Error.IsLocked"); return true; }
    const auto item = repository_.item(id,itemId);
    if (!item) { error(m,"org.freedesktop.Secret.Error.NoSuchObject"); return true; }
    if (m.member() == "GetSecret" && m.signature() == "o") {
        const auto sessionPath = argument<QDBusObjectPath>(m.arguments()[0]);
        auto &s = session(sessionPath.path(),m.service());
        auto wire = s.crypto.encode(sessionPath,item->secret,QString::fromStdString(item->metadata.contentType));
        reply(m,{QVariant::fromValue(wire)}); wipe(wire.value); return true;
    }
    if (m.member() == "SetSecret" && m.signature() == "(oayays)") {
        auto wire = argument<WireSecret>(m.arguments()[0]);
        auto &s = session(wire.session.path(),m.service());
        Item replacement;
        replacement.id = item->id; replacement.attributes = item->attributes; replacement.metadata = item->metadata;
        replacement.metadata.contentType = wire.contentType.toStdString();
        replacement.metadata.modified = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
        replacement.secret = s.crypto.decode(wire);
        if (!repository_.put(id,std::move(replacement))) throw std::runtime_error("Item persistence unavailable");
        signal(collectionPath(id),CollectionInterface,"ItemChanged",{variantPath(m.path())}); reply(m); return true;
    }
    if (m.member() == "Delete" && m.signature().isEmpty()) {
        if (!repository_.erase(id,itemId)) throw std::runtime_error("Item persistence unavailable");
        signal(collectionPath(id),CollectionInterface,"ItemDeleted",{variantPath(m.path())});
        changed(collectionPath(id),CollectionInterface,{{"Items",QVariant::fromValue(paths(id,repository_.search(id,{})))}});
        reply(m,{variantPath()}); return true;
    }
    error(m,"org.freedesktop.DBus.Error.UnknownMethod"); return true;
}
}
