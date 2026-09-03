// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtDBus/QDBusArgument>

namespace QindaQt::Bluetooth
{

// Registers the frozen Bluetooth1 v1 values and the additive Bluetooth2 v2
// values. One process-wide call is required before any value crosses
// QDBusArgument; protocol tests pin both ABIs byte-for-byte.
void registerDBusTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Handle &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Handle &value);

QDBusArgument &operator<<(QDBusArgument &argument, const Adapter &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Adapter &value);

QDBusArgument &operator<<(QDBusArgument &argument, const Device &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Device &value);

QDBusArgument &operator<<(QDBusArgument &argument, const Bluetooth1Device &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Bluetooth1Device &value);

QDBusArgument &operator<<(QDBusArgument &argument, const PairingPrompt &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, PairingPrompt &value);

QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value);

QDBusArgument &operator<<(QDBusArgument &argument, const Bluetooth1Snapshot &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Bluetooth1Snapshot &value);

[[nodiscard]] Bluetooth1Snapshot bluetooth1Projection(const Snapshot &value);

QDBusArgument &operator<<(QDBusArgument &argument, const OperationResult &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, OperationResult &value);

} // namespace QindaQt::Bluetooth
