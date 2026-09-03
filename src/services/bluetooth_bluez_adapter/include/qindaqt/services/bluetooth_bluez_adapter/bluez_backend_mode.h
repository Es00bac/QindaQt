// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QStringView>

namespace QindaQt::Bluetooth
{

// Composition-root selection between the production BlueZ adapter and the B0
// deterministic empty backend. The packaged service defaults to production;
// the deterministic backend exists so qualification can drive the model and
// service machinery without any platform transport.
enum class BluetoothBackendMode {
    Production,
    Deterministic,
};

// AGENT-CONTRACT: The resident service selects its backend exclusively
// through this function from QINDAQT_BLUETOOTH_BACKEND. Only the exact value
// "deterministic" selects the empty backend; every other value, including an
// unset variable, fails closed to production. Case or spelling variants must
// never silently disable the platform adapter.
[[nodiscard]] BluetoothBackendMode resolveBluetoothBackendMode(QStringView value);

} // namespace QindaQt::Bluetooth
