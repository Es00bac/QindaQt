// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
namespace qindaqt::keyring::service {
bool SecretService::nativeItemMethod(const QDBusMessage &message) {
    const auto args=message.arguments();
    if (message.member()=="ListItems" && message.signature()=="o") {
        const auto id=collectionForPath(argument<QDBusObjectPath>(args[0]).path());
        if (id.isEmpty()) throw std::runtime_error("Invalid collection");
        const auto found=repository_.search(id,{});
        protocol::MetadataRows rows;
        for (const auto &value:found.ids) {
            const auto itemId=QString::fromStdString(value);
            QVariantMap row{{"Path",variantPath(itemPath(id,itemId))},
                {"Locked",repository_.locked(id)},{"IndexAuthenticated",found.authenticated}};
            const auto item=repository_.item(id,itemId);
            if (item) {
                row.insert("Label",QString::fromStdString(item->metadata.label));
                row.insert("Created",QVariant::fromValue(static_cast<quint64>(item->metadata.created)));
                row.insert("Modified",QVariant::fromValue(static_cast<quint64>(item->metadata.modified)));
                if (!item->metadata.creator.empty()) row.insert("CreatedBy",QString::fromStdString(item->metadata.creator));
            }
            rows.append(row);
        }
        reply(message,{QVariant::fromValue(rows)});return true;
    }
    if (message.member()=="ReadSecretWithPrompt" && message.signature()=="oo") {
        const auto path=argument<QDBusObjectPath>(args[0]).path();
        const auto id=collectionForPath(path),itemId=itemForPath(path,id);
        const auto sessionPath=argument<QDBusObjectPath>(args[1]).path();
        session(sessionPath,message.service());
        // Volatile collection has no password to reauthenticate. Never substitute
        // an already-unlocked state or unauthenticated index for this gate.
        if (id.isEmpty() || id=="session" || itemId.isEmpty()) throw std::runtime_error("Authentication unavailable");
        Prompt prompt;prompt.owner=message.service();prompt.action="reveal";
        prompt.label=repository_.find(id)->label;prompt.collections={id};
        prompt.objects={objectPath(path)};prompt.secretSession=sessionPath;
        reply(message,{variantPath(addPrompt(std::move(prompt)))});return true;
    }
    if (message.member()=="DeleteItemWithPrompt" && message.signature()=="o") {
        const auto path=argument<QDBusObjectPath>(args[0]).path();
        const auto id=collectionForPath(path),itemId=itemForPath(path,id);
        const auto item=repository_.item(id,itemId);
        if(!item || repository_.locked(id)) throw std::runtime_error("Unlock before deletion");
        Prompt prompt;prompt.owner=message.service();prompt.action="confirm-delete";
        prompt.label=QString::fromStdString(item->metadata.label);
        prompt.collections={id};prompt.objects={objectPath(path)};
        reply(message,{variantPath(addPrompt(std::move(prompt)))});return true;
    }
    if (message.member()=="ChangePasswordWithPrompt" && message.signature()=="o") {
        const auto id=collectionForPath(argument<QDBusObjectPath>(args[0]).path());
        if (id.isEmpty() || id=="session") throw std::runtime_error("Authentication unavailable");
        Prompt prompt;prompt.owner=message.service();prompt.action="change-password";
        prompt.label=repository_.find(id)->label;prompt.collections={id};
        reply(message,{variantPath(addPrompt(std::move(prompt)))});return true;
    }
    return false;
}
}
