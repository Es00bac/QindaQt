// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.SettingsApp.Appearance 1.0

// A theme card that is a rendered thumbnail (ADR-0206): the theme's real
// window chrome painted by the decoration painter over the draft wallpaper,
// with a panel strip in the theme's own panel material. The route model
// publishes every value; the card invents nothing.
T.AbstractButton {
    id: control

    required property string themeName
    property string description: ""
    // The theme's published QST map (DesignTokens::toVariantMap shape).
    property var previewTokens: null
    // The compositor's window chrome for this theme with its paired
    // decoration document; an empty map paints the shipped defaults.
    property var previewChrome: ({})
    property url wallpaper: ""
    property bool available: true

    readonly property var tokens: previewTokens ?? ({})
    readonly property var bg: tokens.bg ?? ({})
    readonly property var fg: tokens.fg ?? ({})
    readonly property var accentTokens: tokens.accent ?? ({})
    readonly property var outline: tokens.outline ?? ({})
    readonly property var panelMaterial: (tokens.material ?? ({})).panel ?? ({})
    readonly property color canvasColor: bg.base ?? Tokens.bg.base
    readonly property color panelColor: bg.raised ?? Tokens.bg.raised

    function tokenColor(value, fallback) {
        return value === undefined ? fallback : value
    }

    checkable: true
    autoExclusive: true
    enabled: available
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: Tokens.space["2"]
    implicitWidth: 196
    implicitHeight: 152
    Accessible.role: Accessible.RadioButton
    Accessible.name: themeName
    Accessible.description: description
    Accessible.checkable: true
    Accessible.checked: checked

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        Item {
            id: thumbnail
            Layout.fillWidth: true
            Layout.preferredHeight: 104
            clip: true

            Rectangle {
                anchors.fill: parent
                radius: Tokens.radius.s
                color: control.canvasColor
            }

            Image {
                anchors.fill: parent
                source: control.wallpaper
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                visible: status === Image.Ready
            }

            AppearanceWindowPreview {
                anchors.fill: parent
                anchors.margins: Tokens.space["1"]
                chrome: control.previewChrome ?? ({})
                canvas: "transparent"
                showInactiveWindow: false
                caption: control.themeName
                toolkitPalette: ({
                    "window": control.tokenColor(control.bg.raised, Tokens.bg.raised),
                    "windowText": control.tokenColor(control.fg["default"], Tokens.fg["default"]),
                    "base": control.tokenColor(control.bg.highest, Tokens.bg.highest),
                    "text": control.tokenColor(control.fg["default"], Tokens.fg["default"]),
                    "button": control.tokenColor(control.bg.highest, Tokens.bg.highest),
                    "buttonText": control.tokenColor(control.fg["default"], Tokens.fg["default"]),
                    "highlight": control.tokenColor(control.accentTokens["default"], Tokens.accent["default"]),
                    "highlightedText": control.tokenColor(control.accentTokens.fg, Tokens.accent.fg),
                    "mid": control.tokenColor(control.outline.divider, Tokens.outline.divider),
                    "dark": control.tokenColor(control.outline.strong, Tokens.outline.strong),
                    "light": control.tokenColor(control.bg.highest, Tokens.bg.highest),
                    "placeholderText": control.tokenColor(control.fg.muted, Tokens.fg.muted),
                    "disabledText": control.tokenColor(control.fg.disabled, Tokens.fg.disabled)
                })
                toolkitFont: Qt.font({ family: Tokens.type.fontFamily, pointSize: 6 })
            }

            // The panel strip carries the theme's own panel material.
            Rectangle {
                objectName: "themeThumbnailPanel"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 12
                color: control.panelColor
                opacity: control.panelMaterial.opacity ?? 1.0
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Text {
                Layout.fillWidth: true
                text: control.themeName
                color: control.enabled ? Tokens.fg["default"] : Tokens.fg.disabled
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: control.checked ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }

            Text {
                visible: control.checked
                text: "✓"
                color: Tokens.fg["default"]
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.subtitle
                font.bold: true
                Accessible.ignored: true
            }
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: control.checked ? Tokens.accent.subtle
                               : control.hovered ? Tokens.state.hover
                                                 : Tokens.bg.raised
        border.width: control.activeFocus || control.checked
                      ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: control.activeFocus ? Tokens.focus.ring
                     : control.checked ? Tokens.accent["default"]
                                       : Tokens.outline.divider
    }
}
