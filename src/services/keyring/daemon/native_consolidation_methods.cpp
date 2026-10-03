// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <set>
#include <stdexcept>

namespace qindaqt::keyring::service {
bool SecretService::consolidationMethod(const QDBusMessage &message) {
    if (message.member() != "ConsolidateCollections" ||
        message.signature() != "oao")
        return false;
    const auto arguments = message.arguments();
    const auto targetPath = argument<QDBusObjectPath>(arguments[0]).path();
    const auto targetId = collectionForPath(targetPath);
    if (targetId.isEmpty() || targetPath != collectionPath(targetId))
        throw std::runtime_error("Invalid collection");

    const auto sourcePaths = argument<Paths>(arguments[1]);
    QStringList sourceIds;
    for (const auto &path : sourcePaths) {
        const auto sourceId = collectionForPath(path.path());
        if (sourceId.isEmpty() || path.path() != collectionPath(sourceId))
            throw std::runtime_error("Invalid collection");
        sourceIds.append(sourceId);
    }
    const auto before = repository_.search(targetId, {}).ids;
    const std::set<std::string> existing(before.begin(), before.end());
    // AGENT-CONTRACT: CollectionRepository owns the sole-writer transaction;
    // no bus reply or collection-deletion signal may precede its durable copy.
    const auto saved = repository_.consolidate(targetId, sourceIds);
    for (const auto &itemId : repository_.search(targetId, {}).ids) {
        if (!existing.contains(itemId))
            signal(collectionPath(targetId), CollectionInterface, "ItemCreated",
                   {variantPath(itemPath(targetId, QString::fromStdString(itemId)))});
    }
    changed(collectionPath(targetId), CollectionInterface,
            {{"Items", QVariant::fromValue(paths(targetId, repository_.search(targetId, {})))}});
    for (const auto &sourceId : sourceIds)
        signal(Root, ServiceInterface, "CollectionDeleted",
               {variantPath(collectionPath(sourceId))});
    changed(Root, ServiceInterface, properties(Root, ServiceInterface));
    reply(message, {saved});
    return true;
}
}
