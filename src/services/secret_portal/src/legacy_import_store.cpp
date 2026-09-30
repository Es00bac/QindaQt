// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/secret_portal/legacy_import.h>
namespace QindaQt::Services::SecretPortal {
LegacyImportCommit commitLegacyImport(qindaqt::keyring::CollectionStore &store,std::vector<LegacyPortalRecord> records) {
    if(store.locked()) return {LegacyImportError::Unavailable,qindaqt::keyring::StoreError::Locked,0,0};
    const auto lookup=[&store](const QString &id) {return store.item(id.toStdString());};
    auto plan=prepareLegacyImport(std::move(records),lookup);
    if(plan.error!=LegacyImportError::None) return {plan.error,qindaqt::keyring::StoreError::None,0,0};
    if(plan.additions.empty()) return {LegacyImportError::None,qindaqt::keyring::StoreError::None,0,plan.unchanged};
    const auto count=plan.additions.size();
    const auto saved=store.insertBatchAndSave(std::move(plan.additions));
    if(saved!=qindaqt::keyring::StoreError::None) return {LegacyImportError::Unavailable,saved,0,0};
    return {LegacyImportError::None,saved,count,plan.unchanged};
}
}
