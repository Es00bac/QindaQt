// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

// One tab of a TabBar: an optional glyph, a short label, and an accent
// indicator when selected. The explanation lives in the tooltip and the
// accessible description, so the strip stays glyph-first.
T.TabButton {
    id: control

    property bool available: true
    property url iconSource: ""
    property int iconSize: 18
    property string accessibleDescription: ""
    readonly property int transitionDuration: Tokens.motion.short

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    // AGENT-CONTRACT: capability is expressed through available, as on
    // Button; overriding inherited enabled replaces this binding.
    enabled: available
    implicitWidth: Math.max(88, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(40, implicitContentHeight + topPadding + bottomPadding)
    leftPadding: Tokens.space["4"]
    rightPadding: Tokens.space["4"]
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["3"]
    spacing: Tokens.space["2"]

    Accessible.role: Accessible.PageTab
    Accessible.name: text
    Accessible.description: accessibleDescription
    Accessible.selected: checked

    ToolTip {
        text: control.accessibleDescription
        visible: control.hovered && control.accessibleDescription.length > 0
    }

    contentItem: Row {
        spacing: control.spacing

        Image {
            objectName: "tabIcon"
            visible: String(control.iconSource).length > 0
            source: control.iconSource
            width: control.iconSize
            height: control.iconSize
            sourceSize.width: control.iconSize
            sourceSize.height: control.iconSize
            anchors.verticalCenter: parent.verticalCenter
            smooth: true
            fillMode: Image.PreserveAspectFit
            opacity: control.enabled ? 1.0 : 0.6
            Accessible.ignored: true
        }

        Text {
            objectName: "tabLabel"
            text: control.text
            anchors.verticalCenter: parent.verticalCenter
            color: !control.enabled ? Tokens.fg.disabled
                  : control.checked ? Tokens.fg.default : Tokens.fg.muted
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            font.weight: control.checked ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
            Accessible.ignored: true

            Behavior on color {
                ColorAnimation { duration: control.transitionDuration }
            }
        }
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.bottomMargin: 3
            radius: Tokens.radius.s
            color: !control.enabled ? "transparent"
                 : control.down ? Tokens.state.pressed
                 : control.hovered ? Tokens.state.hover : "transparent"
            Accessible.ignored: true

            Behavior on color {
                ColorAnimation { duration: control.transitionDuration }
            }
        }

        Rectangle {
            objectName: "tabIndicator"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 3
            radius: 1.5
            color: Tokens.accent.default
            visible: control.checked
            Accessible.ignored: true
        }

        FocusRing {
            objectName: "focusRing"
            anchors.fill: parent
            control: control
        }
    }
}
