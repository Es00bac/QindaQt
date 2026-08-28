// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0

T.AbstractButton {
    id: control

    property string themeName: ""
    property string description: ""
    property var previewTokens: null
    property bool available: true
    property string accessibleDescription: description
    readonly property int transitionDuration: Tokens.motion.short
    readonly property bool previewProvided: previewTokens !== null
                                            && previewTokens !== undefined
    readonly property bool previewValid: !previewProvided
                                         ? true : completePreview(previewTokens)
    readonly property bool previewUnavailable: previewProvided && !previewValid

    function colorComponent(value) {
        return typeof value === "number" && Number.isFinite(value)
                && value >= 0.0 && value <= 1.0
    }

    function colorRole(value) {
        return value !== null && value !== undefined
                && typeof value === "object"
                && colorComponent(value.r) && colorComponent(value.g)
                && colorComponent(value.b) && colorComponent(value.a)
    }

    function completePreview(value) {
        if (value === null || typeof value !== "object"
                || value.bg === null || typeof value.bg !== "object"
                || value.accent === null || typeof value.accent !== "object"
                || value.fg === null || typeof value.fg !== "object"
                || value.outline === null || typeof value.outline !== "object") {
            return false
        }
        return colorRole(value.bg.base) && colorRole(value.bg.raised)
                && colorRole(value.accent.default)
                && colorRole(value.fg.default)
                && colorRole(value.outline.strong)
    }

    function role(group, key) {
        // AGENT-GUARD: Never fall back one role at a time. A partial preview
        // would falsely depict a hybrid of two themes; invalid maps instead
        // take the single explicit unavailable branch below.
        if (!previewProvided)
            return Tokens[group][key]
        if (!completePreview(previewTokens))
            return Tokens.status.warning.foreground
        return previewTokens[group][key]
    }

    checkable: true
    autoExclusive: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    // AGENT-CONTRACT: Consumers express capability through available. Directly
    // overriding inherited enabled replaces the fail-closed preview gate.
    enabled: available && !previewUnavailable
    padding: Tokens.space["4"]
    implicitWidth: 220
    implicitHeight: 132

    Accessible.role: Accessible.RadioButton
    Accessible.name: themeName
    Accessible.description: previewUnavailable
                            ? qsTr("%1 Preview unavailable.").arg(accessibleDescription)
                            : accessibleDescription
    Accessible.checkable: true
    Accessible.checked: checked

    contentItem: ColumnLayout {
        spacing: Tokens.space["3"]

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 48
            radius: Tokens.radius.m
            color: control.previewUnavailable ? Tokens.status.warning.background
                                              : control.role("bg", "base")
            border.width: Tokens.space["1"] / 2
            border.color: control.previewUnavailable ? Tokens.outline.strong
                                                     : control.role("outline", "strong")

            Row {
                anchors.centerIn: parent
                spacing: Tokens.space["2"]

                Repeater {
                    visible: !control.previewUnavailable
                    model: [
                        control.previewUnavailable ? Tokens.status.warning.foreground
                                                   : control.role("bg", "raised"),
                        control.previewUnavailable ? Tokens.status.warning.foreground
                                                   : control.role("accent", "default"),
                        control.previewUnavailable ? Tokens.status.warning.foreground
                                                   : control.role("fg", "default")
                    ]

                    Rectangle {
                        required property color modelData
                        width: 22
                        height: 22
                        radius: Tokens.radius.s
                        color: modelData
                        border.width: Tokens.space["1"] / 2
                        border.color: control.previewUnavailable ? Tokens.outline.strong
                                                                : control.role("outline", "strong")
                        Accessible.ignored: true
                    }
                }

                Text {
                    visible: control.previewUnavailable
                    text: qsTr("Preview unavailable")
                    color: Tokens.status.warning.foreground
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    Accessible.ignored: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["1"]

                Text {
                    Layout.fillWidth: true
                    text: control.themeName
                    color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.body
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Accessible.ignored: true
                }

                Text {
                    Layout.fillWidth: true
                    text: control.description
                    color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    elide: Text.ElideRight
                    Accessible.ignored: true
                }
            }

            Text {
                visible: control.checked
                text: "✓"
                color: Tokens.accent.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.title
                font.bold: true
                Accessible.ignored: true
            }
        }
    }

    background: Rectangle {
        radius: Tokens.radius.l
        color: control.checked ? Tokens.accent.subtle : Tokens.bg.raised
        border.width: control.activeFocus || control.checked ? Tokens.space["1"]
                                                            : Tokens.space["1"] / 2
        border.color: control.activeFocus ? Tokens.focus.ring
                     : control.checked ? Tokens.accent.default : Tokens.outline.divider

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Tokens.state.pressed
                 : control.hovered ? Tokens.state.hover : "transparent"
            Accessible.ignored: true

            Behavior on color {
                ColorAnimation { duration: control.transitionDuration }
            }
        }
    }
}
