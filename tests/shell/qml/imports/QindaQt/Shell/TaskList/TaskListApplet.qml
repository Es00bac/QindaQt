// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production QindaQt.Shell.TaskList
// applet has its own C++ offscreen interaction test; this source module keeps
// the dispatcher mapping test independent of static-plugin registration in
// qmltestrunner.
Item {
    required property var access
    required property var theme
    property bool vertical: false

    objectName: "taskListApplet"
    implicitWidth: 96
    implicitHeight: 32
}
