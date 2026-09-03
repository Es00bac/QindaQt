// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production applet module has its own
// fatal-warning offscreen/keyboard/accessibility rows; this source module
// keeps the dispatcher mapping test independent of static-plugin registration
// in qmltestrunner, mirroring the PowerApplet/GlobalMenu stubs.
Item {
    required property var access
    required property var theme
    property bool vertical: false

    objectName: "statusNotifierApplet"
    implicitWidth: 46
    implicitHeight: 28
    enabled: access !== null
}
