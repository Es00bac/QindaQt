// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.SettingsApp.Appearance 1.0

// One decoration document choice (ADR-0207), previewed through the real
// renderers: a window card paints the document's title bar with the
// decoration painter, a container card the container chrome with the
// compositor's own renderer. Both receive the route model's projections.
T.AbstractButton {
    id: control

    required property string documentName
    property string description: ""
    // "window" paints an application window; "container" a two-member
    // container.
    property string kind: "window"
    property var previewChrome: ({})
    property var previewContainerStyle: ({})
    property var toolkitPalette: ({})
    property font toolkitFont
    property color canvas: Tokens.bg.base
    property url wallpaper: ""
    property bool available: true

    checkable: true
    autoExclusive: true
    enabled: available
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: Tokens.space["2"]
    implicitWidth: 196
    implicitHeight: 142
    Accessible.role: Accessible.RadioButton
    Accessible.name: documentName
    Accessible.description: description
    Accessible.checkable: true
    Accessible.checked: checked
    T.ToolTip.visible: hovered && description.length > 0
    T.ToolTip.delay: 600
    T.ToolTip.text: description

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            sourceComponent: control.kind === "container" ? containerPreview : windowPreview
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Text {
                Layout.fillWidth: true
                text: control.documentName
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

    Component {
        id: windowPreview
        AppearanceWindowPreview {
            chrome: control.previewChrome ?? ({})
            toolkitPalette: control.toolkitPalette ?? ({})
            toolkitFont: control.toolkitFont
            canvas: control.canvas
            wallpaper: control.wallpaper
            showInactiveWindow: false
            caption: control.documentName
        }
    }

    Component {
        id: containerPreview
        AppearanceContainerPreview {
            containerStyle: control.previewContainerStyle ?? ({})
            chrome: control.previewChrome ?? ({})
            toolkitPalette: control.toolkitPalette ?? ({})
            toolkitFont: control.toolkitFont
            canvas: control.canvas
            wallpaper: control.wallpaper
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
