// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production applet has its own C++
// offscreen interaction test; this source module keeps the existing dispatcher
// mapping row independent of static-plugin registration in qmltestrunner.
Item {
    required property var access
    required property var theme
    property bool vertical: false

    objectName: "bluetoothApplet"
    implicitWidth: 52
    implicitHeight: 28
}
