// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production module has its own
// fatal-warning keyboard/accessibility and ownership tests; this fixture
// verifies only the shared panel inventory and facade propagation.
// AGENT-NOTE: added at manager integration of the Global Menu G2 composition,
// whose BuiltinAppletContent import had no test-import stub and broke the
// launcher dispatcher and notification-center offscreen rows.
Item {
    required property var access
    required property var theme
    property bool vertical: false

    objectName: "globalMenuApplet"
    implicitWidth: 64
    implicitHeight: 28
    enabled: access !== null
}
