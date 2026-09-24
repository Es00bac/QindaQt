// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaQt.SettingsApp.Appearance 1.0

// KWin owns the application-window decoration choice (ADR-0160), while
// Settings1 owns QindaQt decoration preferences and compositor container
// chrome (ADR-0129). A foreign plugin is never rendered through QindaQt's
// painter: after apply, this Settings window's own frame is its live preview.
ColumnLayout {
    id: root

    required property var appearanceSettings
    property var windowDecorationSettings: null
    required property bool editorBusy
    readonly property bool editable: appearanceSettings.canEdit && !editorBusy
    readonly property Item firstDecorationChoice: decorationChooser.firstChoice
    readonly property bool qindaQtDecorationSelected:
        windowDecorationSettings === null
        || windowDecorationSettings.selectedUsesQindaQt
    readonly property Item firstFocusTarget: firstDecorationChoice !== null
                                                  ? firstDecorationChoice
                                                  : windowStyleRow.firstChoice
    // ADR-0264: windows and containers offer the same named button styles.
    // AGENT-CONTRACT: tokens mirror ChromePreferences::tokens() for both
    // style keys (chrome_preferences.cpp) and data/settings/schema-v2.json.
    readonly property var buttonStyleChoices: [
        { token: "theme", label: qsTr("Theme") },
        { token: "traffic-lights", label: qsTr("Lights") },
        { token: "flat", label: qsTr("Flat") },
        { token: "glyph", label: qsTr("Glyphs") },
        { token: "gel", label: qsTr("Gel") },
        { token: "bevel", label: qsTr("Bevel") },
        { token: "blue-tiles", label: qsTr("Blue tiles") },
        { token: "wide", label: qsTr("Wide") },
        { token: "tab", label: qsTr("Tab") },
        { token: "bold", label: qsTr("Bold") },
        { token: "minimal", label: qsTr("Minimal") },
        { token: "pills", label: qsTr("Pills") },
        { token: "dots", label: qsTr("Dots") },
        { token: "outline", label: qsTr("Outline") },
        { token: "chunky", label: qsTr("Chunky") }
    ]

    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    WindowDecorationChooser {
        id: decorationChooser
        settings: root.windowDecorationSettings
        editorBusy: root.editorBusy
    }

    SectionHeader {
        objectName: "appearanceWindowsHeader"
        Layout.fillWidth: true
        title: qsTr("Application windows")
        description: root.qindaQtDecorationSelected
                     ? qsTr("QindaQt decoration options")
                     : qsTr("After applying, the Settings window frame is the live preview of the KWin decoration")
    }

    AppearanceWindowPreview {
        objectName: "appearanceWindowChromePreview"
        Layout.fillWidth: true
        visible: root.qindaQtDecorationSelected
        Layout.preferredHeight: Math.round(Math.min(250, Math.max(180, width * 0.4)))
        chrome: root.appearanceSettings.previewChrome ?? ({})
        toolkitPalette: root.appearanceSettings.previewToolkitPalette ?? ({})
        toolkitFont: root.appearanceSettings.previewToolkitFont
                     ?? Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.body })
        canvas: root.appearanceSettings.previewCanvasColor ?? Tokens.bg.base
        wallpaper: root.appearanceSettings.previewWallpaper ?? ""
        caption: qsTr("Report.txt")
        Accessible.role: Accessible.Graphic
        Accessible.name: qsTr("Application window preview")
        Accessible.description: qsTr("Title bar buttons and caption as application windows show them")
    }

    FormSurface {
        objectName: "appearanceExternalDecorationPreview"
        Layout.fillWidth: true
        visible: !root.qindaQtDecorationSelected

        Label {
            width: parent.width
            text: qsTr("This decoration is drawn by KWin, not by QindaQt. Apply it to preview it on this Settings window's real frame.")
            wrapMode: Text.Wrap
            Accessible.name: text
        }
    }

    // The decoration document (ADR-0207) is chosen before the arrangement
    // rows that refine it; "Theme" keeps the color theme's own pairing.
    DecorationDocumentChooser {
        id: windowDocumentChooser
        visible: root.qindaQtDecorationSelected
        appearanceSettings: root.appearanceSettings
        editorBusy: root.editorBusy
        settingsKey: "appearance.windowDecoration"
        kind: "window"
        title: qsTr("Window decoration")
        description: qsTr("Title bar material, corners and shadow for application windows")
        objectPrefix: "appearanceWindowDecorationDocument"
    }

    ChromeMenuChoice {
        id: windowStyleRow
        visible: root.qindaQtDecorationSelected
        label: qsTr("Buttons")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: "appearance.windowButtonStyle"
        choiceObjectName: "appearanceWindowButtonStyle"
        hint: qsTr("How the close, minimize and maximize buttons look")
        choices: root.buttonStyleChoices
    }

    ChromeChoice {
        visible: root.qindaQtDecorationSelected
        label: qsTr("Side")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: "appearance.windowButtonSide"
        choiceObjectName: "appearanceWindowButtonSide"
        hint: qsTr("Which end of the title bar holds the buttons")
        choices: [
            { token: "theme", label: qsTr("Theme") },
            { token: "left", label: qsTr("Left") },
            { token: "right", label: qsTr("Right") }
        ]
    }

    ChromeChoice {
        visible: root.qindaQtDecorationSelected
        label: qsTr("Show")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: "appearance.windowButtons"
        choiceObjectName: "appearanceWindowButtons"
        hint: qsTr("Which buttons title bars offer")
        choices: [
            { token: "all", label: qsTr("All") },
            { token: "minimize-close", label: qsTr("Minimize, close") },
            { token: "close", label: qsTr("Close") }
        ]
    }

    ChromeChoice {
        visible: root.qindaQtDecorationSelected
        label: qsTr("Title")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: "appearance.windowTitleAlignment"
        choiceObjectName: "appearanceWindowTitleAlignment"
        hint: qsTr("Where the window title sits")
        choices: [
            { token: "center", label: qsTr("Center") },
            { token: "left", label: qsTr("Left") }
        ]
    }

    TitleBarOptions {
        visible: root.qindaQtDecorationSelected
        kind: "window"
        appearanceSettings: root.appearanceSettings
        editable: root.editable
    }

    SectionHeader {
        objectName: "appearanceContainersHeader"
        Layout.fillWidth: true
        Layout.topMargin: Tokens.space["3"]
        title: qsTr("Containers")
        description: ""
    }

    AppearanceContainerPreview {
        objectName: "appearanceContainerChromePreview"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.round(Math.min(270, Math.max(200, width * 0.44)))
        containerStyle: root.appearanceSettings.previewContainerStyle ?? ({})
        chrome: root.appearanceSettings.previewChrome ?? ({})
        toolkitPalette: root.appearanceSettings.previewToolkitPalette ?? ({})
        toolkitFont: root.appearanceSettings.previewToolkitFont
                     ?? Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.body })
        canvas: root.appearanceSettings.previewCanvasColor ?? Tokens.bg.base
        wallpaper: root.appearanceSettings.previewWallpaper ?? ""
        Accessible.role: Accessible.Graphic
        Accessible.name: qsTr("Container preview")
        Accessible.description: qsTr("A container of two windows with its tabs and buttons as the desktop draws them")
    }

    DecorationDocumentChooser {
        appearanceSettings: root.appearanceSettings
        editorBusy: root.editorBusy
        settingsKey: "appearance.containerDecoration"
        kind: "container"
        title: qsTr("Container decoration")
        description: qsTr("Chrome material, corners and badge style for containers")
        objectPrefix: "appearanceContainerDecorationDocument"
    }

    ChromeMenuChoice {
        label: qsTr("Buttons")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: "appearance.containerButtonStyle"
        choiceObjectName: "appearanceContainerButtonStyle"
        hint: qsTr("How the buttons on the container bar look")
        choices: root.buttonStyleChoices
    }

    ChromeChoice {
        label: qsTr("Side")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: "appearance.containerButtonSide"
        choiceObjectName: "appearanceContainerButtonSide"
        hint: qsTr("Which end of the container bar holds the buttons")
        choices: [
            { token: "theme", label: qsTr("Theme") },
            { token: "left", label: qsTr("Left") },
            { token: "right", label: qsTr("Right") }
        ]
    }

    ChromeChoice {
        label: qsTr("Tabs")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: "appearance.containerTabOrder"
        choiceObjectName: "appearanceContainerTabOrder"
        hint: qsTr("Whether new tabs line up from the left or from the right")
        choices: [
            { token: "theme", label: qsTr("Theme") },
            { token: "left-to-right", label: qsTr("From left") },
            { token: "right-to-left", label: qsTr("From right") }
        ]
    }

    ChromeChoice {
        label: qsTr("Symbols")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: "appearance.containerButtonGlyphs"
        choiceObjectName: "appearanceContainerButtonGlyphs"
        hint: qsTr("Show button symbols all the time or only under the pointer")
        choices: [
            { token: "theme", label: qsTr("Theme") },
            { token: "always", label: qsTr("Always") },
            { token: "hover", label: qsTr("On hover") }
        ]
    }

    TitleBarOptions {
        kind: "container"
        appearanceSettings: root.appearanceSettings
        editable: root.editable
    }
}
