// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <QUuid>

namespace qindaqt::keyring::service {
bool SecretService::serviceMethod(const QDBusMessage &m) {
    const auto args = m.arguments();
    if (m.member() == "OpenSession" && m.signature() == "sv") {
        const auto algorithm = args[0].toString();
        if (algorithm != "plain" && algorithm != "dh-ietf1024-sha256-aes128-cbc-pkcs7") {
            error(m,"org.freedesktop.DBus.Error.NotSupported"); return true;
        }
        int owned = 0; for (const auto &[path, s] : sessions_) { (void)path; if (s->owner == m.service()) ++owned; }
        if (sessions_.size() >= 256 || owned >= 8) throw std::runtime_error("Session limit");
        const auto input = argument<QDBusVariant>(args[1]).variant();
        if ((algorithm == "plain" && (input.metaType() != QMetaType::fromType<QString>() || !input.toString().isEmpty()))
            || (algorithm != "plain" && input.metaType() != QMetaType::fromType<QByteArray>())) throw std::runtime_error("Invalid negotiation");
        auto session = std::make_unique<Session>(); session->owner = m.service();
        const auto output = session->crypto.negotiate(algorithm,input.toByteArray());
        const auto path = QString(Root) + "/session/s" + QUuid::createUuid().toString(QUuid::Id128);
        sessions_.emplace(path,std::move(session));
        reply(m,{QVariant::fromValue(QDBusVariant(algorithm == "plain" ? QVariant(QString()) : QVariant(output))),variantPath(path)});
        return true;
    }
    if (m.member() == "CreateCollection" && m.signature() == "a{sv}s") {
        const auto properties = argument<QVariantMap>(args[0]);
        const auto alias = args[1].toString();
        const auto label = properties.value(QString(CollectionInterface) + ".Label").toString();
        if (properties.size() > 1 || label.size() > 1024 || label.toUtf8().size() > 1024 || (!alias.isEmpty() && !validName(alias)))
            throw std::runtime_error("Invalid collection");
        const auto existing = repository_.alias(alias);
        if (!existing.isEmpty()) {
            if (repository_.locked(existing)) { error(m,"org.freedesktop.Secret.Error.IsLocked"); return true; }
            if (!properties.isEmpty()) repository_.setLabel(existing,label);
            reply(m,{variantPath(collectionPath(existing)),variantPath()}); return true;
        }
        Prompt prompt; prompt.owner = m.service(); prompt.action = "create"; prompt.label = label; prompt.alias = alias;
        const auto path = addPrompt(std::move(prompt));
        reply(m,{variantPath(),variantPath(path)}); return true;
    }
    if (m.member() == "SearchItems" && m.signature() == "a{ss}") {
        const auto query = attributes(argument<StringMap>(args[0]));
        Paths unlocked, locked;
        for (const auto &id : repository_.names()) {
            auto &destination = repository_.locked(id) ? locked : unlocked;
            destination.append(paths(id,repository_.search(id,query)));
        }
        reply(m,{QVariant::fromValue(unlocked),QVariant::fromValue(locked)}); return true;
    }
    if ((m.member() == "Lock" || m.member() == "Unlock") && m.signature() == "ao") {
        const auto objects = argument<Paths>(args[0]);
        if (objects.size() > 256) throw std::runtime_error("Object limit");
        Prompt prompt; prompt.owner = m.service(); prompt.action = "unlock"; prompt.objects = objects;
        Paths completed;
        for (const auto &path : objects) if (!exists(path.path()) || collectionForPath(path.path()).isEmpty()) {
            error(m,"org.freedesktop.Secret.Error.NoSuchObject"); return true;
        }
        for (const auto &path : objects) {
            const auto id = collectionForPath(path.path());
            if (id.isEmpty() || (path.path() != collectionPath(id)
                && !path.path().startsWith(QString(Root) + "/aliases/") && itemForPath(path.path(),id).isEmpty())) {
                error(m,"org.freedesktop.Secret.Error.NoSuchObject"); return true;
            }
            if (m.member() == "Lock") {
                cancelRelatedUnlock();
                repository_.lock(id);
                notifyCollectionState(id);
                completed.append(path);
            } else if (!repository_.locked(id) || (id == "session" && repository_.unlock(id,{}))) completed.append(path);
            else if (!prompt.collections.contains(id)) prompt.collections.append(id);
        }
        const auto path = prompt.collections.isEmpty() ? QString("/") : addPrompt(std::move(prompt));
        reply(m,{QVariant::fromValue(completed),variantPath(path)}); return true;
    }
    if (m.member() == "ReadAlias" && m.signature() == "s") {
        const auto id = repository_.alias(args[0].toString());
        reply(m,{variantPath(id.isEmpty() ? "/" : collectionPath(id))}); return true;
    }
    if (m.member() == "SetAlias" && m.signature() == "so") {
        const auto path = argument<QDBusObjectPath>(args[1]).path();
        const auto id = path == "/" ? QString() : collectionForPath(path);
        if (path != "/" && (id.isEmpty() || path != collectionPath(id))) throw std::runtime_error("Invalid alias");
        repository_.setAlias(args[0].toString(),id); reply(m); return true;
    }
    if (m.member() == "GetSecrets" && m.signature() == "aoo") {
        const auto requested = argument<Paths>(args[0]); if (requested.size() > 256) throw std::runtime_error("Object limit");
        const auto sessionPath = argument<QDBusObjectPath>(args[1]);
        auto &s = session(sessionPath.path(),m.service());
        SecretMap results;
        for (const auto &path : requested) {
            const auto id = collectionForPath(path.path());
            const auto itemId = itemForPath(path.path(),id);
            const auto item = repository_.item(id,itemId);
            if (item && !repository_.locked(id))
                results.insert(path,s.crypto.encode(sessionPath,item->secret,QString::fromStdString(item->metadata.contentType)));
        }
        reply(m,{QVariant::fromValue(results)});
        for (auto i = results.begin(); i != results.end(); ++i) wipe(i->value);
        return true;
    }
    error(m,"org.freedesktop.DBus.Error.UnknownMethod"); return true;
}
}
