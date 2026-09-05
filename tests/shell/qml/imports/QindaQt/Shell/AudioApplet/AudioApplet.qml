// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production applet has its own C++
// offscreen interaction test; this source module keeps the dispatcher mapping
// test independent of static-plugin registration in qmltestrunner.
Item {
    required property var controller
    property bool vertical: false

    objectName: "audioApplet"
    implicitWidth: 46
    implicitHeight: 28
}
