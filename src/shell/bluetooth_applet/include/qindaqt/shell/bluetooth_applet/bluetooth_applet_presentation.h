// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h>

#include <optional>

namespace QindaQt::Shell::BluetoothApplet
{

// Projects one bounded, owned value model. The caller proves that `snapshot`
// belongs to the exact currently bound service owner; loss of that proof makes
// the result unavailable and drops all prior inventory.
[[nodiscard]] BluetoothAppletModel projectBluetoothApplet(
    const Bluetooth::Snapshot &snapshot,
    bool exactOwnerAvailable,
    bool readGranted,
    bool controlGranted,
    std::optional<Bluetooth::Handle> discoveryLease = std::nullopt);

} // namespace QindaQt::Shell::BluetoothApplet
