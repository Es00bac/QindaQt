// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/secret_portal/legacy_import.h>
#include <filesystem>
#include <sys/stat.h>
using namespace QindaQt::Services::SecretPortal;
using namespace qindaqt::keyring;
// Private compiled synthetic records only, created before the fixture daemon's
// exclusive writer starts. No wallet reader, environment credentials or logs.
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    try {
        const std::string root=argv[1];if(!std::filesystem::is_directory(root)) std::filesystem::create_directory(root);chmod(root.c_str(),0700);
        CollectionStore store(root,"login");const unsigned char password[]="synthetic-keyring-password";
        if(store.create({password,sizeof(password)-1},{8192,1,1})!=StoreError::None || store.save()!=StoreError::None) return 3;
        LegacyPortalRecord record;record.appId="org.example.Legacy";record.sourceWallet="synthetic-network-wallet";record.secret=SecureBuffer(64);
        for(std::size_t index=0;index<64;++index) record.secret.bytes()[index]=static_cast<unsigned char>((index*17U)&255U);
        std::vector<LegacyPortalRecord> records;records.emplace_back(std::move(record));const auto result=commitLegacyImport(store,std::move(records));if(result.error!=LegacyImportError::None || result.added!=1) return 4;
        auto forged=newApplicationSecret("org.example.Forged");forged.attributes["qindaqt.portal.version"]="legacy-opaque-64";forged.secret=SecureBuffer(64);
        if(store.put(std::move(forged))!=StoreError::None || store.save()!=StoreError::None) return 5;
        return 0;
    } catch(const std::exception &) {return 6;}
}
