// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "native_radio_wire_p.h"
#include <qindaqt/services/bluetooth_radio_helper/radio_types.h>
namespace QindaQt::BluetoothRadio {
bool appendNativeRequest(DBusMessage *message, const Request &request);
bool readNativeRequest(DBusMessage *message, Request *request);
bool appendNativeResult(DBusMessage *message, const Result &result);
bool readNativeResult(DBusMessage *message, Result *result);
bool appendNativeText(DBusMessageIter *iter, const QString &text);
QString readNativeText(DBusMessageIter *iter);
} // namespace QindaQt::BluetoothRadio
