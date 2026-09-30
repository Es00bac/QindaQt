// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/legacy_snapshot.h>
#include <qindaqt/services/secret_portal/legacy_import.h>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <set>
#include <algorithm>
namespace qindaqt::keyring {
namespace {
QString hash(const QString &text) {return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(),QCryptographicHash::Sha256).toHex().left(39));}
bool safe(const QString &id) {
    static const QRegularExpression name("^[A-Za-z0-9_.-]{1,40}$");
    return id!="." && id!=".." && id!="session" && name.match(id).hasMatch();
}
bool aliasName(const QString &id) {
    static const QRegularExpression name("^[A-Za-z0-9_.-]{1,60}$");
    return id!="." && id!=".." && name.match(id).hasMatch();
}
bool bounded(const QString &value,qsizetype maximum,bool empty=false) {
    return (empty || !value.isEmpty()) && !value.contains(QChar(0)) && value.toUtf8().size()<=maximum
        && QString::fromUtf8(value.toUtf8())==value;
}
SecureBuffer clone(const SecureBuffer &source) {
    SecureBuffer copy(source.size());std::copy(source.bytes().begin(),source.bytes().end(),copy.bytes().begin());return copy;
}
}
LegacyCollectionPlan planLegacyCollections(std::vector<LegacySnapshot> sources) {
    using namespace QindaQt::Services::SecretPortal;
    LegacyCollectionPlan result;
    const auto reject=[&](CollectionImportError error) {result.batch={};result.error=error;return std::move(result);};
    if(sources.empty() || sources.size()>2) return reject(CollectionImportError::InvalidInput);
    std::set<QString> kinds,nativeIds;std::vector<LegacyPortalRecord> portalRecords;
    std::map<std::pair<QString,QString>,QString> names;
    try {
        // Secret Service names are selected first, independent of caller order.
        std::stable_sort(sources.begin(),sources.end(),[](const auto &a,const auto &b){return a.kind<b.kind;});
        for(auto &source:sources) {
            std::sort(source.collections.begin(),source.collections.end(),[](const auto &a,const auto &b){return a.sourceId<b.sourceId;});
            if((source.kind!="secret-service" && source.kind!="kwallet") || !kinds.insert(source.kind).second)
                return reject(CollectionImportError::InvalidInput);
        }
        for(const auto &kind:{QString("secret-service"),QString("kwallet")}) for(const auto &source:sources) if(source.kind==kind) {
            for(const auto &collection:source.collections) {
                if(!bounded(collection.sourceId,1024) || names.contains({kind,collection.sourceId})) return reject(CollectionImportError::InvalidInput);
                const auto candidate=kind=="secret-service"?collection.sourceId.section('/',-1):collection.sourceId;
                QString id=candidate;
                if(!safe(id) || nativeIds.contains(id) || (kind=="kwallet" && id=="login"))
                    id=(kind=="kwallet"?"k":"s")+hash(kind+":"+collection.sourceId);
                if(!nativeIds.insert(id).second) return reject(CollectionImportError::Conflict);
                names.emplace(std::make_pair(kind,collection.sourceId),id);
            }
        }
        if(names.size()>64) return reject(CollectionImportError::Capacity);
        for(auto &source:sources) {
            for(auto &collection:source.collections) {
                if(!bounded(collection.label,1024,true) || collection.items.size()>1024) return reject(CollectionImportError::Capacity);
                CollectionImportRecord record;record.id=names.at({source.kind,collection.sourceId});record.label=collection.label;
                record.sourceKind=source.kind;record.sourceId=collection.sourceId;record.created=collection.created;record.modified=collection.modified;
                std::set<std::pair<QString,QString>> originalItems;std::set<std::string> itemIds;std::size_t aggregate=4;
                for(auto &original:collection.items) {
                    if(!bounded(original.sourceId,1024) || !originalItems.emplace(original.folder,original.sourceId).second)
                        return reject(CollectionImportError::InvalidInput);
                    Item item;item.id=("i"+hash(source.kind=="kwallet"?original.folder+QChar(0)+original.sourceId:original.sourceId)).toStdString();
                    if(!itemIds.insert(item.id).second) return reject(CollectionImportError::Conflict);
                    item.metadata=std::move(original.metadata);item.attributes=std::move(original.attributes);
                    // Standard Secret Service has no Creator property. This field
                    // preserves the complete source item ID, not caller authority.
                    item.metadata.creator=original.sourceId.toStdString();
                    if(source.kind=="kwallet") {
                        if(original.entryType<1 || original.entryType>3 || !item.attributes.empty() || !bounded(original.folder,1024,true))
                            return reject(CollectionImportError::InvalidInput);
                        item.attributes={{"qindaqt.import.kwallet.folder",original.folder.toStdString()},
                            {"qindaqt.import.kwallet.key",original.sourceId.toStdString()},
                            {"qindaqt.import.kwallet.type",std::to_string(original.entryType)}};
                        if(original.folder=="xdg-desktop-portal") {
                            if(original.entryType!=2 || original.secret.size()!=64 || portalRecords.size()>=128)
                                return reject(CollectionImportError::InvalidInput);
                            portalRecords.push_back({original.sourceId,collection.sourceId,clone(original.secret)});
                        }
                    }
                    item.secret=std::move(original.secret);
                    if(item.secret.size()>1024*1024 || item.attributes.size()>32 || item.metadata.label.size()>1024
                        || item.metadata.contentType.size()>128) return reject(CollectionImportError::Capacity);
                    aggregate+=40+item.id.size()+item.secret.size()+item.metadata.label.size()+item.metadata.contentType.size()+item.metadata.creator.size();
                    for(const auto &[key,value]:item.attributes) {
                        if(key.empty() || key.size()>1024 || value.size()>1024) return reject(CollectionImportError::Capacity);
                        aggregate+=8+key.size()+value.size();
                    }
                    if(aggregate>4*1024*1024) return reject(CollectionImportError::Capacity);
                    record.items.push_back(std::move(item));
                }
                result.batch.collections.push_back(std::move(record));
            }
            for(auto alias=source.aliases.cbegin();alias!=source.aliases.cend();++alias) {
                if(!aliasName(alias.key()) || !names.contains({source.kind,alias.value()})) return reject(CollectionImportError::InvalidInput);
                const auto id=names.at({source.kind,alias.value()});
                if(result.batch.aliases.contains(alias.key()) && result.batch.aliases.value(alias.key())!=id)
                    return reject(CollectionImportError::Conflict);
                result.batch.aliases[alias.key()]=id;
            }
        }
        if(!portalRecords.empty()) {
            auto portal=prepareLegacyImport(std::move(portalRecords),[](const QString &)->const Item *{return nullptr;});
            if(portal.error!=LegacyImportError::None) return reject(portal.error==LegacyImportError::Unavailable
                ?CollectionImportError::Unavailable:CollectionImportError::Conflict);
            auto login=std::find_if(result.batch.collections.begin(),result.batch.collections.end(),[](const auto &c){return c.id=="login";});
            if(login==result.batch.collections.end()) {
                if(result.batch.collections.size()>=64) return reject(CollectionImportError::Capacity);
                CollectionImportRecord c;c.id="login";c.label="Login";c.sourceKind="portal-derived";c.sourceId="kwallet-secret-portal-v1";
                result.batch.collections.push_back(std::move(c));login=std::prev(result.batch.collections.end());
            }
            for(auto &item:portal.additions) {
                // KWallet has no entry timestamps. Stable unknown dates make
                // exact collection retry independent of the import wall clock.
                item.metadata.created=item.metadata.modified=0;
                if(std::any_of(login->items.begin(),login->items.end(),[&](const auto &old){return old.id==item.id;}))
                    return reject(CollectionImportError::Conflict);
                login->items.push_back(std::move(item));
            }
            if(login->items.size()>1024) return reject(CollectionImportError::Capacity);
        }
        for(const auto &record:result.batch.collections) {
            std::size_t aggregate=4;
            for(const auto &item:record.items) {
                aggregate+=40+item.id.size()+item.secret.size()+item.metadata.label.size()+item.metadata.contentType.size()+item.metadata.creator.size();
                for(const auto &[key,value]:item.attributes) aggregate+=8+key.size()+value.size();
            }
            if(aggregate>4*1024*1024) return reject(CollectionImportError::Capacity);
        }
        if(result.batch.collections.empty()) return reject(CollectionImportError::InvalidInput);
        return result;
    } catch(const std::exception &) {return reject(CollectionImportError::Unavailable);}
}
}
