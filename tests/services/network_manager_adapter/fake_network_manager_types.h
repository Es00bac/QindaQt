// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusObjectPath>

namespace QindaQt::Network::NetworkManager::TestSupport {

using SettingsMap = QMap<QString, QVariantMap>;
using Interfaces = QMap<QString, QVariantMap>;
using ObjectTree = QMap<QDBusObjectPath, Interfaces>;

inline void registerSettingsMap() {
  qRegisterMetaType<SettingsMap>();
  qDBusRegisterMetaType<SettingsMap>();
  qDBusRegisterMetaType<ObjectTree>();
}

} // namespace QindaQt::Network::NetworkManager::TestSupport
