// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production QindaQt.Shell.StartMenu
// applet has its own C++ offscreen test (tests/shell/start_menu); this source
// module keeps the dispatcher mapping test independent of static-plugin
// registration in qmltestrunner. It mirrors the production boundary exactly:
// applet/theme/vertical plus the two borrowed facades.
Item {
    property var applet: null
    property var theme: null
    property bool vertical: false
    property var launcherAppletAccess: null
    property var desktopControlsAccess: null

    objectName: "startMenuApplet"
    implicitWidth: 56
    implicitHeight: 36
    enabled: launcherAppletAccess !== null
    Accessible.role: Accessible.Button
    Accessible.name: "Start"
}
