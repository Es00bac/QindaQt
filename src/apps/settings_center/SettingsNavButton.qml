// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls

Controls.Button {
    id: control

    property bool active: false
    property bool routeAvailable: true
    property string routeId: ""
    property string category: ""
    property string routeDescription: ""
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

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: control.text
                color: !control.routeAvailable ? Tokens.fg.disabled
                     : control.active ? Tokens.accent.fg
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
