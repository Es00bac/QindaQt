// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtDBus/QDBusArgument>

namespace QindaQt::Bluetooth
{

void registerDBusTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Handle &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Handle &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Adapter &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Adapter &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Device &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Device &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value);
QDBusArgument &operator<<(QDBusArgument &argument, const OperationResult &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, OperationResult &value);

} // namespace QindaQt::Bluetooth
