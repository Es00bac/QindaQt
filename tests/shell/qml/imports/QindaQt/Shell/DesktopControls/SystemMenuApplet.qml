// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double for the compiled QindaQt.Shell.DesktopControls
// module (see tests/shell/qml/imports/QindaQt/Shell/GlobalMenu for the
// precedent). It mirrors the production boundary exactly: `access` and
// `vertical` only; tokens come from QindaQt.Tokens, never a theme map.
Item {
    required property var access
    property bool vertical: false

    objectName: "systemMenuApplet"
    implicitWidth: 48
    implicitHeight: 28
    enabled: access !== null
}
