// SPDX-License-Identifier: GPL-3.0-or-later
//
// Private helpers shared by the production upstream adapters. Nothing here is
// installed or part of the module's public boundary.

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusConnection>

#include <functional>

namespace QindaQt::Power::Upstream {

// Extracts an array of object paths ('ao') from a reply argument that QtDBus
// delivered as a QDBusArgument because no typed QList operator exists.
[[nodiscard]] bool readObjectPathArray(const QVariant &value, QStringList &paths);

// Extracts an array of a{sv} dictionaries ('aa{sv}') from a reply argument.
[[nodiscard]] bool readStringVariantMapArray(const QVariant &value,
                                             QList<QVariantMap> &entries);

// Reads one boolean property out of an untrusted a{sv} property map. A
// missing or wrongly typed property yields false without failing the whole
// read; the caller decides whether that is fatal for its domain.
[[nodiscard]] bool optionalBool(const QVariantMap &properties, const QString &name,
                                bool &value);

// Issues one asynchronous org.freedesktop.DBus.Properties.GetAll and dispatches
// exactly one of the two callbacks on the context object's thread. The accept
// callback also receives the replying unique bus name so callers can fence
// operations against upstream owner replacement.
void getAllProperties(const QDBusConnection &connection, const QString &service,
                      const QString &path, const QString &interface, QObject *context,
                      const std::function<void(const QVariantMap &,
                                               const QString &sender)> &accept,
                      const std::function<void(const QString &)> &reject);

} // namespace QindaQt::Power::Upstream
