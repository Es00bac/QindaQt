// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/collection_import.h>
#include "../daemon/collection_repository.h"
#include <stdexcept>
namespace qindaqt::keyring {
namespace {
class Catalog final:public CollectionImportCatalog {
public:
    Catalog(const QString &directory,KdfParameters parameters):repository_(directory),parameters_(parameters) {}
    CollectionImportReceipt commit(CollectionImportBatch batch,CollectionImportPasswords &passwords,
            const std::function<bool()> &admitted) override {
        if(active_) return {CollectionImportError::Unavailable};
        active_=true;
        struct Reset {bool &active;~Reset(){active=false;}} reset{active_};
        return repository_.importCollections(std::move(batch),passwords,admitted,parameters_);
    }
private:
    service::CollectionRepository repository_;
    KdfParameters parameters_;
    bool active_=false;
};
}
std::unique_ptr<CollectionImportCatalog> openCollectionImportCatalog(const QString &directory,KdfParameters parameters) {
    try {return std::make_unique<Catalog>(directory,parameters);}
    catch(const std::exception &) {throw std::runtime_error("Import storage unavailable");}
}
}
