// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/collection_store.h>
#include <QString>
#include <QMap>
#include <functional>
#include <memory>
#include <vector>
namespace qindaqt::keyring {
// Source IDs/labels remain distinct from bounded native filename IDs. These
// values record provenance; they do not independently authenticate a provider.
struct CollectionImportRecord {
    QString id,label,sourceKind,sourceId;
    std::uint64_t created=0,modified=0;
    std::vector<Item> items;
};
struct CollectionImportBatch {
    std::vector<CollectionImportRecord> collections;
    QMap<QString,QString> aliases;
};
enum class CollectionImportError {None,InvalidInput,Capacity,Conflict,Cancelled,AuthenticationFailed,OwnerLost,Unavailable,DurabilityUnknown};
struct CollectionImportReceipt {
    CollectionImportError error=CollectionImportError::None;
    StoreError persistence=StoreError::None;
    std::size_t collectionsAdded=0,itemsAdded=0,collectionsUnchanged=0,itemsUnchanged=0;
};
struct ImportPassword {
    CollectionImportError error=CollectionImportError::None;
    SecureBuffer value;
};
// Same-thread borrowed provider, outlives commit. Returns owned locked pages,
// cancellation or a sanitized failure; never supplies credentials in argv/logs.
// Existing means authenticate an idempotence check, not permission to overwrite.
class CollectionImportPasswords {
public:
    virtual ~CollectionImportPasswords()=default;
    virtual ImportPassword take(const QString &id,const QString &label,bool existing)=0;
};
// One-time importer seam; no resident bulk mutation service. Consumes all pages.
// Borrowed admission callback is readonly/non-reentrant and joins accepted
// source/session lifetime; it is checked before staging and catalog publication.
// At most 64 collections, existing storage item/aggregate bounds, exact-existing
// idempotence or whole-batch conflict. One durable catalog publication exposes
// all new files. Unknown durability reloads locked and never reports success.
class CollectionImportCatalog {
public:
    virtual ~CollectionImportCatalog()=default;
    virtual CollectionImportReceipt commit(CollectionImportBatch,CollectionImportPasswords &,
        const std::function<bool()> &admitted)=0;
};
// Owns the native private catalog/storage adapter and exclusive writer lease.
// Absolute directory is constructor-visible; schema remains private to keyring.
// Throws sanitized runtime_error for unavailable/unsafe paths or writer conflict.
// Must be destroyed before starting the resident or switching the secrets name.
std::unique_ptr<CollectionImportCatalog> openCollectionImportCatalog(const QString &absoluteDirectory,KdfParameters={});
}
