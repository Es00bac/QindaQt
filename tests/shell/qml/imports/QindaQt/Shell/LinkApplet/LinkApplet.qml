// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production applet has its own C++
// offscreen interaction test; this source module keeps the dispatcher mapping
// tests independent of static-plugin registration in qmltestrunner.
// AGENT-GUARD: `access` must stay non-required, matching the other stubs, so
// panel rows that never supply a link access object keep constructing.
Item {
    property var access: null
    required property var theme
    property bool vertical: false

    objectName: "linkApplet"
    implicitWidth: 32
    implicitHeight: 28
}
