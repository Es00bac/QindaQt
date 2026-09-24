// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0

// Title-bar options for one chrome set (ADR-0264): button size, spacing and
// the title double-click for windows and containers alike, plus the
// window-only title height, corners, title weight, app icon and roll-up
// button. A container bar's height is part of the container layout, so it
// has no height option. Keys are "appearance.<kind>…", as in the schema.
ColumnLayout {
    id: root

    required property var appearanceSettings
    required property bool editable
    // "window" or "container": the Settings1 key family of this set.
    required property string kind
    readonly property bool windows: kind === "window"
    readonly property string keyPrefix: "appearance." + kind
    readonly property string objectPrefix: windows ? "appearanceWindow" : "appearanceContainer"
    readonly property var switchChoices: [
        { token: "hidden", label: qsTr("Hide") },
        { token: "shown", label: qsTr("Show") }
    ]

    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    ChromeChoice {
        label: qsTr("Button size")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: root.keyPrefix + "ButtonSize"
        choiceObjectName: root.objectPrefix + "ButtonSize"
        hint: qsTr("Smaller or larger buttons")
        choices: [
            { token: "theme", label: qsTr("Theme") },
            { token: "small", label: qsTr("Small") },
            { token: "large", label: qsTr("Large") }
        ]
    }

    ChromeChoice {
        label: qsTr("Button spacing")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: root.keyPrefix + "ButtonSpacing"
        choiceObjectName: root.objectPrefix + "ButtonSpacing"
        hint: qsTr("How far apart the buttons sit")
        choices: [
            { token: "theme", label: qsTr("Theme") },
            { token: "tight", label: qsTr("Tight") },
            { token: "roomy", label: qsTr("Roomy") }
        ]
    }

    // Window-only rows, built only for windows so the container instance
    // never carries rows (or object names) for keys it does not have.
    Loader {
        Layout.fillWidth: true
        active: root.windows
        visible: active
        sourceComponent: ColumnLayout {
            spacing: Tokens.space["3"]

            ChromeChoice {
                label: qsTr("Title bar")
                settings: root.appearanceSettings
                canEdit: root.editable
                settingsKey: "appearance.windowTitleHeight"
                choiceObjectName: "appearanceWindowTitleHeight"
                hint: qsTr("How tall title bars are")
                choices: [
                    { token: "theme", label: qsTr("Theme") },
                    { token: "compact", label: qsTr("Compact") },
                    { token: "tall", label: qsTr("Tall") }
                ]
            }

            ChromeChoice {
                label: qsTr("Corners")
                settings: root.appearanceSettings
                canEdit: root.editable
                settingsKey: "appearance.windowCornerRadius"
                choiceObjectName: "appearanceWindowCornerRadius"
                hint: qsTr("How round window corners are")
                choices: [
                    { token: "theme", label: qsTr("Theme") },
                    { token: "square", label: qsTr("Square") },
                    { token: "small", label: qsTr("Small") },
                    { token: "large", label: qsTr("Large") }
                ]
            }

            ChromeChoice {
                label: qsTr("Title weight")
                settings: root.appearanceSettings
                canEdit: root.editable
                settingsKey: "appearance.windowTitleWeight"
                choiceObjectName: "appearanceWindowTitleWeight"
                hint: qsTr("Regular or bold window titles")
                choices: [
                    { token: "theme", label: qsTr("Theme") },
                    { token: "regular", label: qsTr("Regular") },
                    { token: "bold", label: qsTr("Bold") }
                ]
            }

            ChromeChoice {
                label: qsTr("App icon")
                settings: root.appearanceSettings
                canEdit: root.editable
                settingsKey: "appearance.windowAppIcon"
                choiceObjectName: "appearanceWindowAppIcon"
                hint: qsTr("Show the application's icon beside the window title")
                choices: root.switchChoices
            }

            ChromeChoice {
                label: qsTr("Roll-up button")
                settings: root.appearanceSettings
                canEdit: root.editable
                settingsKey: "appearance.windowRollUpButton"
                choiceObjectName: "appearanceWindowRollUpButton"
                hint: qsTr("A button that rolls the window up to its icon")
                choices: root.switchChoices
            }
        }
    }

    ChromeChoice {
        label: qsTr("Double-click")
        settings: root.appearanceSettings
        canEdit: root.editable
        settingsKey: root.keyPrefix + "TitleDoubleClick"
        choiceObjectName: root.objectPrefix + "TitleDoubleClick"
        hint: root.windows ? qsTr("What double-clicking a title bar does")
                           : qsTr("What double-clicking the container bar does")
        // Windows default to KWin's own double-click (maximize unless the
        // user changed it); a container bar did nothing before ADR-0264.
        choices: [
            root.windows ? { token: "theme", label: qsTr("Default") }
                         : { token: "none", label: qsTr("Nothing") },
            { token: "maximize", label: qsTr("Maximize") },
            { token: "roll-up", label: qsTr("Roll up") },
            { token: "minimize", label: qsTr("Minimize") }
        ]
    }
}
