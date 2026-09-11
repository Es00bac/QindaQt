// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls
import QindaQt.Shell.Icons 1.0 as ShellIcons

Controls.Button {
    id: control

    property bool active: false
    property bool routeAvailable: true
    property string routeId: ""
    property string category: ""
    property string routeDescription: ""
    // XDG icon name from the route descriptor; glyph-first navigation.
    property string iconName: ""
    property string unavailableReason: ""

    objectName: routeId.length > 0 ? ("settingsNavButton_" + routeId) : "settingsNavButton"

    hoverEnabled: true
    emphasized: active
    implicitWidth: 180
    implicitHeight: 44
    leftPadding: Tokens.space["3"]
    rightPadding: Tokens.space["3"]
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["2"]

    Accessible.role: Accessible.PageTab
    Accessible.name: text
    // AGENT-CONTRACT: An unavailable PageTab remains keyboard-focusable so
    // assistive technology can discover why it is unavailable and Escape can
    // return from its fail-closed page. Its unavailable diagnostic and guarded
    // activation express capability without disabling Qt focus.
    Accessible.description: routeAvailable ? routeDescription
        : qsTr("Unavailable. %1").arg(unavailableReason.length > 0
                                      ? unavailableReason : routeDescription)
    Accessible.selected: active
    Controls.ToolTip {
        text: control.routeAvailable ? control.routeDescription
                                     : control.unavailableReason
        visible: control.hovered && text.length > 0
    }

    contentItem: RowLayout {
        spacing: Tokens.space["2"]

        Rectangle {
            id: activeIndicator
            implicitWidth: 3
            Layout.fillHeight: true
            radius: 1.5
            color: control.active ? Tokens.accent.default : "transparent"
            visible: control.active
            Accessible.ignored: true
        }

        ShellIcons.Icon {
            objectName: "settingsNavIcon"
            visible: control.iconName.length > 0
            name: control.iconName
            size: 20
            symbolic: true
            // AGENT-GUARD: recolor must be fully opaque; the disabled state
            // dims through opacity instead of a translucent token.
            color: control.routeAvailable ? Tokens.fg.default : Tokens.fg.muted
            opacity: control.routeAvailable ? 1.0 : 0.6
            fallbackText: control.text
            Layout.alignment: Qt.AlignVCenter
            Accessible.ignored: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: control.text
                color: !control.routeAvailable ? Tokens.fg.disabled
                     // AGENT-GUARD: The selected navigation background is the
                     // raised surface, so its label must use the normal
                     // foreground. accent.fg is paired only with
                     // accent.default and becomes white on white in Qinda
                     // Light when used here.
                     : control.active ? Tokens.fg.default
                     : Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: control.active ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                Accessible.ignored: true
            }

            Text {
                Layout.fillWidth: true
                visible: control.category.length > 0 && !control.active
                text: control.category
                color: Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                elide: Text.ElideRight
                Accessible.ignored: true
            }
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: control.active ? Tokens.bg.raised
             : control.routeAvailable && control.down ? Tokens.state.pressed
             : control.routeAvailable && control.hovered ? Tokens.state.hover
             : "transparent"

        Controls.FocusRing {
            objectName: "focusRing"
            anchors.fill: parent
            control: control
        }
    }
}
