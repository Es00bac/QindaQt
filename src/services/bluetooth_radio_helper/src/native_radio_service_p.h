// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "radio_operation_p.h"
#include "native_radio_wire_p.h"
namespace QindaQt::BluetoothRadio {
// Borrows both owners; remove native callback before either is destroyed.
class NativeRadioService final {
public:
    NativeRadioService(NativeRadioWire &wire, RadioOperation &operation);
    ~NativeRadioService();
private:
    bool receive(DBusMessage *message);
    NativeRadioWire &m_wire;
    RadioOperation &m_operation;
    bool m_busy = false;
};
} // namespace QindaQt::BluetoothRadio
