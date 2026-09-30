// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/collection_store.h>
#include <QString>
#include <QVariantMap>
namespace QindaQt::Services::SecretPortal {
inline constexpr auto FrontendName="org.freedesktop.portal.Desktop";
inline constexpr auto BackendName="org.freedesktop.impl.portal.desktop.qindaqt";
inline constexpr auto SecretInterface="org.freedesktop.impl.portal.Secret";
inline constexpr auto RequestInterface="org.freedesktop.impl.portal.Request";
inline constexpr std::size_t SecretSize=32;
// Pure bounded validation. app_id is supplied ONLY by the authenticated portal
// frontend, never inferred from a process or payload of an ordinary app caller.
// Empty host IDs fail closed; nonempty host IDs retain frontend host semantics.
bool validApplicationId(const QString &);
bool validRequestHandle(const QString &);
bool validOptions(const QVariantMap &);
QString applicationItemId(const QString &);
// Keyring owns collection selection/persistence. Domain-specific records cannot
// substitute unrelated items, forged metadata, or a store encryption key.
qindaqt::keyring::Item newApplicationSecret(const QString &);
bool matchesApplicationSecret(const qindaqt::keyring::Item &,const QString &);
}
