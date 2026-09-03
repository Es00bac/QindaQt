// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_protocol/clipboard_protocol.h>

#include <QtDBus/QDBusArgument>

namespace QindaQt::Services::ClipboardModel {

QDBusArgument &operator<<(QDBusArgument &argument, const EntryId &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, EntryId &value);

} // namespace QindaQt::Services::ClipboardModel

namespace QindaQt::Services::Clipboard {

void registerDBusTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value);
QDBusArgument &operator<<(QDBusArgument &argument, const OperationResult &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, OperationResult &value);

} // namespace QindaQt::Services::Clipboard
