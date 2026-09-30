// SPDX-License-Identifier: GPL-3.0-or-later
#include "legacy_wire_p.h"
#include <openssl/crypto.h>
#include <stdexcept>
namespace qindaqt::keyring::importer {
bool sameSnapshot(const LegacySnapshot &a,const LegacySnapshot &b) {
    if(a.kind!=b.kind || a.aliases!=b.aliases || a.collections.size()!=b.collections.size()) return false;
    for(std::size_t n=0;n<a.collections.size();++n) {
        const auto &x=a.collections[n];const auto &y=b.collections[n];
        if(x.sourceId!=y.sourceId || x.label!=y.label || x.created!=y.created || x.modified!=y.modified || x.items.size()!=y.items.size()) return false;
        for(std::size_t m=0;m<x.items.size();++m) {
            const auto &p=x.items[m];const auto &q=y.items[m];
            if(p.sourceId!=q.sourceId || p.folder!=q.folder || p.entryType!=q.entryType || p.attributes!=q.attributes
                || p.metadata.label!=q.metadata.label || p.metadata.contentType!=q.metadata.contentType
                || p.metadata.created!=q.metadata.created || p.metadata.modified!=q.metadata.modified
                || p.secret.size()!=q.secret.size() || (p.secret.size() && CRYPTO_memcmp(p.secret.bytes().data(),q.secret.bytes().data(),p.secret.size())!=0)) return false;
        }
    }
    return true;
}
class Reader final:public LegacyReader {
public:
    Reader(const QString &address,LegacySourceBinding binding,const std::function<bool()> &admitted)
        :wire_(address,std::move(binding),admitted) {}
    LegacyReadReceipt acquire(const QStringList &aliases) override {
        if(active_ || done_) return {CollectionImportError::Unavailable,LegacySnapshot{}};
        active_=true;done_=true;
        struct Reset {bool &active;~Reset(){active=false;}} reset{active_};
        try {
            auto first=read(aliases);auto second=read(aliases);
            if(!wire_.live()) return {CollectionImportError::OwnerLost,LegacySnapshot{}};
            if(!sameSnapshot(first,second)) return {CollectionImportError::Conflict,LegacySnapshot{}};
            wire_.freeze();
            if(!wire_.live()) return {CollectionImportError::OwnerLost,LegacySnapshot{}};
            return {CollectionImportError::None,std::move(second)};
        } catch(const Failure &failure) {return {failure.error,LegacySnapshot{}};}
        catch(const std::exception &) {return {CollectionImportError::Unavailable,LegacySnapshot{}};}
    }
    bool live() override {return wire_.live();}
private:
    LegacySnapshot read(const QStringList &aliases) {return wire_.binding().kind=="kwallet"?readKWallet(wire_):readSecretService(wire_,aliases);}
    Wire wire_;bool active_=false,done_=false;
};
}
namespace qindaqt::keyring {
std::unique_ptr<LegacyReader> openLegacyReader(const QString &address,LegacySourceBinding binding,const std::function<bool()> &admitted) {
    try {return std::make_unique<importer::Reader>(address,std::move(binding),admitted);}
    catch(...) {throw std::runtime_error("Legacy source admission unavailable");}
}
}
