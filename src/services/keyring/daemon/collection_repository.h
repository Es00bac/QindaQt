// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "private_directory.h"
#include "wire_types.h"
#include <qindaqt/services/keyring/collection_store.h>
#include <qindaqt/services/keyring/collection_import.h>
#include <QElapsedTimer>
#include <map>
namespace qindaqt::keyring::service {
struct Collection {
    QString id, label, importKind, importSourceId;
    quint64 created = 0, modified = 0;
    std::unique_ptr<CollectionStore> storage;
    std::map<std::string, Item> volatileItems;
    bool volatileLocked = false;
};
// Thread-confined sole writer. Returned views are borrowed until mutation,
// lock/load or destruction, never across an event-loop boundary. Password
// spans are borrowed only for each call; successful writes mean durable save.
// Ordinary sanitized errors reject; PersistenceError requires broker exit/reload.
class CollectionRepository final {
public:
    explicit CollectionRepository(QString directory);
    QStringList names() const;
    Collection *find(const QString &id);
    bool locked(const QString &id) const;
    SearchResult search(const QString &id, const Attributes &query) const;
    const Item *item(const QString &id, const QString &itemId) const;
    QString create(QString label, QString alias, std::span<const unsigned char> password);
    bool unlock(const QString &id, std::span<const unsigned char> password);
    bool rekey(const QString &id, std::span<const unsigned char> password);
    void lock(const QString &id);
    bool put(const QString &id, Item item);
    bool erase(const QString &id, const QString &itemId);
    void remove(const QString &id);
    // Copy first under the resident sole-writer lease, then retire unlocked
    // source collections. A failed copy leaves every source intact; an
    // interrupted retirement can be retried without duplicating item IDs.
    unsigned int consolidate(const QString &targetId, const QStringList &sourceIds);
    QString alias(const QString &name) const;
    void setAlias(const QString &name, const QString &id);
    CollectionImportReceipt importCollections(CollectionImportBatch,CollectionImportPasswords &,const std::function<bool()> &,KdfParameters,const std::function<void()> &);
    void setLabel(const QString &id, const QString &label);
private:
    void loadCatalog();
    QByteArray catalogBytes() const;
    void persistCatalog();
    bool kdfAllowed();
    QString directory_;
    PrivateDirectory files_;
    std::map<QString, std::unique_ptr<Collection>> collections_;
    QMap<QString, QString> aliases_;
    QElapsedTimer kdfClock_;
    bool importHealthy_=true;
};
Attributes attributes(const StringMap &wire);
StringMap attributes(const Attributes &stored);
SecureBuffer copySecret(const SecureBuffer &source);
bool validName(const QString &id);
}
