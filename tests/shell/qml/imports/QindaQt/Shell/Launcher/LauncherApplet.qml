// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production module has a separate
// fatal-warning keyboard/accessibility test; this fixture verifies only the
// shared panel inventory and purpose-specific facade propagation.
Item {
    required property var access
    property bool vertical: false

    objectName: "launcherApplet"
    implicitWidth: 64
    implicitHeight: 28
    enabled: access !== null
}
