// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusObjectPath>

namespace QindaQt::Network::NetworkManager::TestSupport {

struct DeviceStateReason final {
  quint32 state = 0;
  quint32 reason = 0;
};
inline QDBusArgument &operator<<(QDBusArgument &argument, const DeviceStateReason &value) {
  argument.beginStructure();
  argument << value.state << value.reason;
  argument.endStructure();
  return argument;
}
inline const QDBusArgument &operator>>(const QDBusArgument &argument, DeviceStateReason &value) {
  argument.beginStructure();
  argument >> value.state >> value.reason;
  argument.endStructure();
  return argument;
}

using SettingsMap = QMap<QString, QVariantMap>;
using Interfaces = QMap<QString, QVariantMap>;
using ObjectTree = QMap<QDBusObjectPath, Interfaces>;

inline void registerSettingsMap() {
  qDBusRegisterMetaType<DeviceStateReason>();
  qRegisterMetaType<SettingsMap>();
  qDBusRegisterMetaType<SettingsMap>();
  qDBusRegisterMetaType<ObjectTree>();
}

} // namespace QindaQt::Network::NetworkManager::TestSupport

Q_DECLARE_METATYPE(QindaQt::Network::NetworkManager::TestSupport::DeviceStateReason)
