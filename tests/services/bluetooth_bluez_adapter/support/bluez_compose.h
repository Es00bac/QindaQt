// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QHash>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusObjectPath>

namespace QindaQt::Tests
{

// Exact BlueZ wire shapes expressed through registered Qt container types so
// QtDBus marshals the real signatures (a{sv}, a{sa{sv}}, a{oa{sa{sv}}})
// instead of hand-composed QDBusArgument values.
using FakeBluezInterfaces = QHash<QString, QVariantMap>;
using FakeBluezObjectTree = QHash<QDBusObjectPath, FakeBluezInterfaces>;

inline const QMetaType fakeBluezObjectTreeMetaType()
{
    return QMetaType::fromType<FakeBluezObjectTree>();
}

} // namespace QindaQt::Tests

Q_DECLARE_METATYPE(QindaQt::Tests::FakeBluezObjectTree)
