// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtDBus/QDBusArgument>

namespace QindaQt::Audio
{

// Registers only fixed Audio1 structs. No generic QVariant dictionary is part
// of the public domain model.
void registerDBusTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Handle &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Handle &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Device &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Device &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Stream &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Stream &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value);
QDBusArgument &operator<<(QDBusArgument &argument, const OperationResult &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, OperationResult &value);

} // namespace QindaQt::Audio
