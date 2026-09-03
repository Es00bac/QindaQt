// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/bluetooth_bluez_adapter/bluez_backend_mode.h>

namespace QindaQt::Bluetooth
{

BluetoothBackendMode resolveBluetoothBackendMode(const QStringView value)
{
    // AGENT-GUARD: Fail closed to the production adapter. A typo, a casing
    // variant, or an unset variable must never silently downgrade the
    // resident service to the empty deterministic backend.
    if (value == QStringView(u"deterministic")) {
        return BluetoothBackendMode::Deterministic;
    }
    return BluetoothBackendMode::Production;
}

} // namespace QindaQt::Bluetooth
