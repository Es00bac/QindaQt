// SPDX-License-Identifier: GPL-3.0-or-later
#include "collection_repository.h"
#include "../src/format_p.h"
#include <openssl/crypto.h>
#include <algorithm>
#include <set>
namespace qindaqt::keyring::service {
namespace {
bool live(const std::function<bool()> &admitted) noexcept {
    try {return admitted && admitted();} catch(...) {return false;}
}
bool same(const Item &a,const Item &b) {
    return a.id==b.id && a.attributes==b.attributes && a.metadata.label==b.metadata.label
        && a.metadata.contentType==b.metadata.contentType && a.metadata.creator==b.metadata.creator
        && a.metadata.created==b.metadata.created && a.metadata.modified==b.metadata.modified
        && a.secret.size()==b.secret.size() && (a.secret.size()==0
            || CRYPTO_memcmp(a.secret.bytes().data(),b.secret.bytes().data(),a.secret.size())==0);
}
bool valid(const CollectionImportRecord &record) {
    if(!validName(record.id) || record.id=="session" || record.label.contains(QChar(0))
        || record.label.toUtf8().size()>1024 || record.sourceId.isEmpty()
        || record.sourceId.contains(QChar(0)) || record.sourceId.toUtf8().size()>1024
        || (record.sourceKind!="secret-service" && record.sourceKind!="kwallet" && record.sourceKind!="portal-derived")
        || record.items.size()>detail::MaxItems) return false;
    std::set<std::string> ids;std::size_t total=4;
    for(const auto &item:record.items) {
        if(!detail::validItem(item) || !ids.insert(item.id).second) return false;
        total+=40+item.id.size()+item.secret.size()+item.metadata.label.size()
            +item.metadata.contentType.size()+item.metadata.creator.size();
        for(const auto &[key,value]:item.attributes) total+=8+key.size()+value.size();
        if(total>detail::MaxPlain) return false;
    }
    return true;
}
}
CollectionImportReceipt CollectionRepository::importCollections(CollectionImportBatch batch,
        CollectionImportPasswords &passwords,const std::function<bool()> &admitted,KdfParameters parameters) {
    using Error=CollectionImportError;
    if(!importHealthy_) return {Error::Unavailable};
    if(!live(admitted)) return {Error::OwnerLost};
    if(batch.collections.empty() || batch.collections.size()>64 || batch.aliases.size()>64)
        return {Error::InvalidInput};
    std::set<QString> ids;std::set<std::pair<QString,QString>> sources;
    for(const auto &record:batch.collections)
        if(!valid(record) || !ids.insert(record.id).second
            || !sources.emplace(record.sourceKind,record.sourceId).second) return {Error::InvalidInput};
    std::size_t added=0;
    for(const auto &id:ids) if(!find(id)) ++added;
    if(collections_.size()-1+added>64) return {Error::Capacity};
    auto mergedAliases=aliases_;
    for(auto i=batch.aliases.cbegin();i!=batch.aliases.cend();++i) {
        if(!validName(i.key()) || !ids.contains(i.value())) return {Error::InvalidInput};
        if(mergedAliases.contains(i.key()) && mergedAliases.value(i.key())!=i.value()) return {Error::Conflict};
        mergedAliases[i.key()]=i.value();
    }
    if(mergedAliases.size()>64) return {Error::Capacity};
    CollectionImportReceipt receipt;
    std::map<QString,std::unique_ptr<Collection>> pending;
    QStringList attempted;
    const auto oldAliases=aliases_;
    bool published=false;
    // AGENT-GUARD: only new unreferenced files are removed on rollback. Existing
    // catalog-referenced collections are authenticated/read/locked, never saved.
    const auto rollback=[&] {
        aliases_=oldAliases;
        for(const auto &[id,collection]:pending) {(void)collection;collections_.erase(id);}
        const auto disk=files_.collections();
        for(const auto &id:attempted) if(disk.contains(id)) files_.remove(id+".qkr");
    };
    const auto reject=[&](Error error,StoreError persistence=StoreError::None) {
        rollback();return CollectionImportReceipt{error,persistence};
    };
    try {
        // Validate and authenticate every existing collection before staging.
        for(auto &record:batch.collections) {
            if(!live(admitted)) return reject(Error::OwnerLost);
            auto existing=find(record.id);
            if(existing && (existing->importKind!=record.sourceKind || existing->importSourceId!=record.sourceId
                || existing->label!=record.label || existing->created!=record.created || existing->modified!=record.modified))
                return reject(Error::Conflict);
            auto password=passwords.take(record.id,record.label,existing!=nullptr);
            if(password.error!=Error::None) return reject(password.error);
            if(password.value.size()==0 || password.value.size()>4096) return reject(Error::InvalidInput);
            if(!live(admitted)) return reject(Error::OwnerLost);
            if(existing) {
                const auto result=existing->storage->unlock(password.value.bytes());
                if(result!=StoreError::None) return reject(Error::AuthenticationFailed,result);
                struct Lock {CollectionStore &store;~Lock(){store.lock();}} lock{*existing->storage};
                const auto index=existing->storage->search({});
                bool exact=index.authenticated && index.ids.size()==record.items.size();
                for(const auto &item:record.items) {
                    const auto old=existing->storage->item(item.id);
                    if(!old || !same(*old,item)) {exact=false;break;}
                }
                if(!exact) return reject(Error::Conflict);
                ++receipt.collectionsUnchanged;receipt.itemsUnchanged+=record.items.size();
            } else {
                auto candidate=std::make_unique<Collection>();
                candidate->id=record.id;candidate->label=record.label;candidate->importKind=record.sourceKind;
                candidate->importSourceId=record.sourceId;candidate->created=record.created;candidate->modified=record.modified;
                candidate->storage=std::make_unique<CollectionStore>(directory_.toStdString(),record.id.toStdString(),
                    [&admitted]{return live(admitted);});
                const auto result=candidate->storage->create(password.value.bytes(),parameters);
                if(result!=StoreError::None) return reject(Error::Unavailable,result);
                pending.emplace(record.id,std::move(candidate));
            }
        }
        for(auto &record:batch.collections) {
            const auto i=pending.find(record.id);if(i==pending.end()) continue;
            if(!live(admitted)) return reject(Error::OwnerLost);
            if(files_.collections().contains(record.id)) return reject(Error::Conflict);
            const auto count=record.items.size();attempted.append(record.id);
            const auto result=record.items.empty()?i->second->storage->save()
                :i->second->storage->insertBatchAndSave(std::move(record.items));
            i->second->storage->lock();
            if(result!=StoreError::None) return reject(result==StoreError::DurabilityUnknown?Error::DurabilityUnknown
                :live(admitted)?Error::Unavailable:Error::OwnerLost,result);
            ++receipt.collectionsAdded;receipt.itemsAdded+=count;
        }
        if(!live(admitted)) return reject(Error::OwnerLost);
        if(pending.empty() && mergedAliases==aliases_) return receipt;
        for(auto &[id,collection]:pending) collections_.emplace(id,std::move(collection));
        aliases_=std::move(mergedAliases);
        const auto result=files_.replaceForImport("catalog.json",catalogBytes(),[&admitted]{return live(admitted);});
        if(result==StoreError::DurabilityUnknown) {
            published=true;loadCatalog();return {Error::DurabilityUnknown,result};
        }
        if(result!=StoreError::None) return reject(live(admitted)?Error::Unavailable:Error::OwnerLost,result);
        published=true;return receipt;
    } catch(const std::exception &) {
        if(!published) {try {rollback();} catch(const std::exception &) {}}
        // A cleanup/load failure is never a successful or retryable publication.
        try {loadCatalog();} catch(const std::exception &) {importHealthy_=false;}
        return {published?Error::DurabilityUnknown:Error::Unavailable,StoreError::IoError};
    }
}
}
