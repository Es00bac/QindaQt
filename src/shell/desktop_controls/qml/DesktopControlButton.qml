// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Icon-first panel summary button shared by the desktop controls. It owns
// presentation only: the caller binds availability, the accessible strings,
// and the `triggered` handler. Keyboard activation covers Space (inherited),
// Return, and Enter so the layer-shell panel's Tab traversal has an action.
T.ToolButton {
    id: control

    property string iconName: ""
    property string fallbackText: ""
    property string labelText: ""
    property bool showLabel: false
    property bool available: true
    property bool vertical: false
    property bool active: false
    property string accessibleDescription: ""
    property int iconExtent: Math.max(0, Math.min(20, height - Tokens.space["2"]))

    signal triggered()

    enabled: available
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    padding: Tokens.space["1"]
    text: ""
    implicitWidth: Math.max(28, contentRow.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 28

    Accessible.role: Accessible.Button
    Accessible.description: accessibleDescription

    onClicked: control.triggered()
    Keys.onReturnPressed: if (enabled) control.triggered()
    Keys.onEnterPressed: if (enabled) control.triggered()
    Accessible.onPressAction: if (enabled) control.triggered()

    contentItem: RowLayout {
        id: contentRow
        spacing: Tokens.space["2"]

        ShellIcons.Icon {
            objectName: "desktopControlIcon"
            Layout.alignment: Qt.AlignVCenter
            name: control.iconName
            size: control.iconExtent
            color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
            symbolic: true
            fallbackText: control.fallbackText
            Accessible.ignored: true
        }

        Text {
            objectName: "desktopControlLabel"
            Layout.alignment: Qt.AlignVCenter
            visible: control.showLabel && !control.vertical && text.length > 0
            text: control.labelText
            color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            elide: Text.ElideRight
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: control.down ? Tokens.state.pressed
             : control.hovered ? Tokens.state.hover
             : control.active ? Tokens.bg.raised
             : "transparent"
        border.width: control.active ? Tokens.space["1"] / 2 : 0
        border.color: Tokens.outline.divider

        C.FocusRing {
            anchors.fill: parent
            control: control
        }
    }
}
