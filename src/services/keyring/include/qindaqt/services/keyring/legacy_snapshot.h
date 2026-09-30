// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/collection_import.h>
namespace qindaqt::keyring {
struct LegacyItemSnapshot {
    QString sourceId,folder;
    int entryType=0; // KWallet:1 password,2 stream,3 map;0 is not an entry.
    ItemMetadata metadata;
    Attributes attributes;
    SecureBuffer secret;
};
struct LegacyCollectionSnapshot {
    QString sourceId,label;
    std::uint64_t created=0,modified=0;
    std::vector<LegacyItemSnapshot> items;
};
struct LegacySnapshot {
    QString kind; // secret-service or kwallet; accepted by the owning reader.
    std::vector<LegacyCollectionSnapshot> collections;
    QMap<QString,QString> aliases; // Original alias -> original collection ID.
};
struct LegacyCollectionPlan {
    CollectionImportError error=CollectionImportError::None;
    CollectionImportBatch batch;
};
// Pure same-thread consuming planner. No provider authentication, storage, GUI
// or service lookup. Preserve original IDs in provenance/creator metadata and
// exact bytes/types/attributes; map unsafe/colliding native names deterministically.
// Retain all KWallet entries AND derive opaque64 portal records into native login.
// Capacity/conflicts fail the entire plan and wipe its provisional pages.
LegacyCollectionPlan planLegacyCollections(std::vector<LegacySnapshot>);
}
