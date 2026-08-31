// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/portal/appearance_policy.h"

#include <QDBusArgument>
#include <QMap>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Services::Portal {

QDBusArgument &operator<<(QDBusArgument &argument,
                          const PortalAccentColor &color);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                PortalAccentColor &color);

namespace Private {

using PortalNamespaceMap = QMap<QString, QVariantMap>;

void registerPortalDBusTypes();

} // namespace Private
} // namespace QindaQt::Services::Portal

Q_DECLARE_METATYPE(QindaQt::Services::Portal::Private::PortalNamespaceMap)
