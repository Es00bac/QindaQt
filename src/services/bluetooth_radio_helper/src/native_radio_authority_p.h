// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "radio_operation_p.h"
#include "native_radio_wire_p.h"
namespace QindaQt::BluetoothRadio {
// Same-thread borrowed native peers retain real sender on every reply/error.
// Delegation is issued by current Bluetooth1 for the entire pending intent;
// caller strings, UID or PID alone cannot grant it.
class NativeRadioAuthority final : public RadioAuthority {
public:
    NativeRadioAuthority(NativeRadioWire &session, NativeRadioWire &bluez);
    bool current(const QString &sender, const Request &request) override;
private:
    bool lineage(const QString &sender, const Request &request);
    bool intent(const Request &request);
    NativeRadioWire &m_session, &m_bluez;
};
} // namespace QindaQt::BluetoothRadio
