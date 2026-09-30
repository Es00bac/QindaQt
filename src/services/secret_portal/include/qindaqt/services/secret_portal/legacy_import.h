// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <functional>
#include <vector>
namespace QindaQt::Services::SecretPortal {
inline constexpr std::size_t LegacySecretSize=64;
struct LegacyPortalRecord {
    QString appId;
    // Non-secret source wallet identity, supplied by the authorized acquisition
    // boundary. This policy neither opens a wallet nor authenticates provenance.
    QString sourceWallet;
    qindaqt::keyring::SecureBuffer secret;
};
enum class LegacyImportError {None,InvalidInput,Conflict,Unavailable};
struct LegacyImportPlan {
    LegacyImportError error=LegacyImportError::None;
    std::vector<qindaqt::keyring::Item> additions;
    std::size_t unchanged=0;
};
// Same-thread pure policy; consumes/wipes rejected record pages. Borrowed lookup
// is readonly/non-reentrant and returns currently admitted collection items.
// At most128 records; a duplicate or conflict rejects the whole plan. Existing
// exact opaque64 bytes + identical provenance is idempotent, never overwritten.
// Nothing here grants caller authority, unlocks, saves, chooses a path or logs.
LegacyImportPlan prepareLegacyImport(std::vector<LegacyPortalRecord>,
    const std::function<const qindaqt::keyring::Item *(const QString &itemId)> &lookup);
struct LegacyImportCommit {
    LegacyImportError error=LegacyImportError::None;
    qindaqt::keyring::StoreError persistence=qindaqt::keyring::StoreError::None;
    std::size_t added=0,unchanged=0;
};
// Separate synchronous persistence collaborator over the public store API.
// Caller owns admitted fixed-login selection/unlock/exclusive writer lifetime.
// Nothing is written for rejected/idempotent plans. A durable successful commit
// alone reports additions; unknown durability leaves the store reloaded locked.
LegacyImportCommit commitLegacyImport(qindaqt::keyring::CollectionStore &,std::vector<LegacyPortalRecord>);
bool matchesLegacySecret(const qindaqt::keyring::Item &,const QString &appId);
bool supportedPortalSecret(const qindaqt::keyring::Item &,const QString &appId);
}
