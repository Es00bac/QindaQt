// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/secret_portal/legacy_import.h>
#include <QDateTime>
#include <openssl/crypto.h>
#include <set>
namespace QindaQt::Services::SecretPortal {
namespace {
bool validWallet(const QString &wallet) {
    if(wallet.isEmpty() || wallet.toUtf8().size()>255 || QString::fromUtf8(wallet.toUtf8())!=wallet) return false;
    for(const auto character:wallet) if(character.category()==QChar::Other_Control) return false;
    return true;
}
qindaqt::keyring::Attributes provenance(const QString &app,const QString &wallet) {
    return {{"qindaqt.portal.application",app.toStdString()},{"qindaqt.portal.version","legacy-opaque-64"},
        {"qindaqt.portal.source.schema","kwallet-secret-portal-v1"},{"qindaqt.portal.source.folder","xdg-desktop-portal"},{"qindaqt.portal.source.wallet",wallet.toStdString()}};
}
}
bool matchesLegacySecret(const qindaqt::keyring::Item &item,const QString &app) {
    const auto found=item.attributes.find("qindaqt.portal.source.wallet");if(found==item.attributes.end()) return false;
    const auto wallet=QString::fromStdString(found->second);
    return validApplicationId(app) && validWallet(wallet) && item.id==applicationItemId(app).toStdString()
        && item.attributes==provenance(app,wallet) && item.secret.size()==LegacySecretSize
        && item.metadata.contentType=="application/vnd.qindaqt.portal-secret";
}
bool supportedPortalSecret(const qindaqt::keyring::Item &item,const QString &app) {
    return matchesApplicationSecret(item,app) || matchesLegacySecret(item,app);
}
LegacyImportPlan prepareLegacyImport(std::vector<LegacyPortalRecord> records,
    const std::function<const qindaqt::keyring::Item *(const QString &)> &lookup) {
    LegacyImportPlan plan;
    const auto reject=[&](LegacyImportError error) {plan.additions.clear();plan.unchanged=0;plan.error=error;};
    if(records.empty() || records.size()>128 || !lookup) {reject(LegacyImportError::InvalidInput);return plan;}
    std::set<QString> identities;
    try {
        for(auto &record:records) {
            if(!validApplicationId(record.appId) || !validWallet(record.sourceWallet) || record.secret.size()!=LegacySecretSize || !identities.insert(record.appId).second) {
                reject(LegacyImportError::InvalidInput);return plan;
            }
            const auto id=applicationItemId(record.appId);
            const auto attributes=provenance(record.appId,record.sourceWallet);
            if(const auto *existing=lookup(id)) {
                if(!matchesLegacySecret(*existing,record.appId) || existing->attributes!=attributes
                    || CRYPTO_memcmp(existing->secret.bytes().data(),record.secret.bytes().data(),LegacySecretSize)!=0) {
                    reject(LegacyImportError::Conflict);return plan;
                }
                ++plan.unchanged;continue;
            }
            qindaqt::keyring::Item item;item.id=id.toStdString();item.attributes=attributes;item.secret=std::move(record.secret);
            item.metadata.label="Application secret";item.metadata.contentType="application/vnd.qindaqt.portal-secret";item.metadata.creator="QindaQt legacy Secret import";
            item.metadata.created=item.metadata.modified=static_cast<std::uint64_t>(QDateTime::currentSecsSinceEpoch());plan.additions.emplace_back(std::move(item));
        }
    } catch(const std::exception &) {reject(LegacyImportError::Unavailable);}
    return plan;
}
}
