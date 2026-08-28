// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtDBus/QDBusArgument>

namespace QindaQt::Power {

// Registers only fixed structs and arrays. Power1 v1 intentionally exposes no
// generic QVariant dictionary because an open property bag would bypass its
// schema, bounds, and total-decoding contract.
void registerDBusTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Handle &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Handle &value);
QDBusArgument &operator<<(QDBusArgument &argument, const SourceTruth &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                SourceTruth &value);
QDBusArgument &operator<<(QDBusArgument &argument, const PowerSupply &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                PowerSupply &value);
QDBusArgument &operator<<(QDBusArgument &argument,
                          const CompositeBattery &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                CompositeBattery &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Profile &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Profile &value);
QDBusArgument &operator<<(QDBusArgument &argument, const ProfileHold &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                ProfileHold &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Inhibitor &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Inhibitor &value);
QDBusArgument &operator<<(QDBusArgument &argument,
                          const KeyboardBacklight &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                KeyboardBacklight &value);
QDBusArgument &operator<<(QDBusArgument &argument,
                          const InternalBacklight &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                InternalBacklight &value);
QDBusArgument &operator<<(QDBusArgument &argument, const WaylandBinding &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                WaylandBinding &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value);
QDBusArgument &operator<<(QDBusArgument &argument,
                          const OperationResult &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                OperationResult &value);

} // namespace QindaQt::Power
