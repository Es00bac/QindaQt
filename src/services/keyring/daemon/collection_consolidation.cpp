// SPDX-License-Identifier: GPL-3.0-or-later
#include "collection_repository.h"
#include <QDateTime>
#include <openssl/crypto.h>
#include <map>
#include <set>
#include <stdexcept>

namespace qindaqt::keyring::service {
namespace {
bool identical(const Item &left, const Item &right) {
    const auto &a = left.metadata, &b = right.metadata;
    if (a.label != b.label || a.contentType != b.contentType ||
        a.creator != b.creator || a.created != b.created ||
        a.modified != b.modified || left.attributes != right.attributes ||
        left.secret.size() != right.secret.size())
        return false;
    return left.secret.size() == 0 ||
        CRYPTO_memcmp(left.secret.bytes().data(), right.secret.bytes().data(),
                      left.secret.size()) == 0;
}
}

unsigned int CollectionRepository::consolidate(const QString &targetId,
                                               const QStringList &sourceIds) {
    auto *target = find(targetId);
    if (!target || !target->storage || locked(targetId) ||
        sourceIds.isEmpty() || sourceIds.size() > 63)
        throw std::runtime_error("Consolidation unavailable");

    std::set<QString> uniqueSources;
    std::map<std::string, const Item *> uniqueItems;
    unsigned int sourceItemCount = 0;
    for (const auto &sourceId : sourceIds) {
        auto *source = find(sourceId);
        if (sourceId == targetId || !uniqueSources.insert(sourceId).second ||
            !source || !source->storage || locked(sourceId))
            throw std::runtime_error("Consolidation unavailable");
        // AGENT-GUARD: removing an aliased collection must not silently
        // invalidate a client-facing alias. This operation only admits
        // unaliased imported wallets; the default login alias is retained.
        for (auto alias = aliases_.cbegin(); alias != aliases_.cend(); ++alias)
            if (alias.value() == sourceId)
                throw std::runtime_error("Consolidation unavailable");
        // A failed index build reports no IDs. Treating that as an empty
        // collection would retire the only encrypted copy of its items.
        const auto sourceItems = source->storage->search({});
        if (!sourceItems.authenticated)
            throw std::runtime_error("Consolidation unavailable");
        for (const auto &itemId : sourceItems.ids) {
            const auto *item = source->storage->item(itemId);
            if (!item)
                throw std::runtime_error("Consolidation unavailable");
            ++sourceItemCount;
            const auto [found, inserted] = uniqueItems.emplace(itemId, item);
            if (!inserted && !identical(*found->second, *item))
                throw std::runtime_error("Consolidation conflict");
        }
    }

    std::vector<Item> additions;
    additions.reserve(uniqueItems.size());
    for (const auto &[id, original] : uniqueItems) {
        if (const auto *existing = target->storage->item(id)) {
            if (!identical(*existing, *original))
                throw std::runtime_error("Consolidation conflict");
            continue; // A prior copy committed before source retirement.
        }
        Item copied;
        copied.id = original->id;
        copied.metadata = original->metadata;
        copied.attributes = original->attributes;
        copied.secret = copySecret(original->secret);
        additions.push_back(std::move(copied));
    }

    if (!additions.empty()) {
        // AGENT-CONTRACT: insertion is one durable encrypted-file commit.
        // Every source remains visible until it succeeds; a crash during
        // later catalog retirement can be retried by exact item identity.
        const auto result = target->storage->insertBatchAndSave(std::move(additions));
        if (result == StoreError::DurabilityUnknown)
            throw PersistenceError();
        if (result != StoreError::None)
            throw std::runtime_error("Consolidation unavailable");
        target->modified = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
        persistCatalog();
    }
    for (const auto &sourceId : sourceIds)
        remove(sourceId);
    return sourceItemCount;
}
}
