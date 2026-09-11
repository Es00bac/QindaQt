// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaQt.Shell.Icons 1.0 as ShellIcons

// Night light section for the Display route: on/off, schedule, temperatures,
// transition length, and live compositor truth in one short status line.
// Explanations live in tooltips, never in paragraphs. Every control is
// keyboard reachable with an accessible name; everything disables when the
// compositor's night light service is unavailable (fail closed, ADR-0136).
// The location, custom-times, and temperature rows live in their own files
// beside this one.
ColumnLayout {
    id: root

    required property var nightLight

    readonly property bool controlsEnabled: root.nightLight.available
                                            && !root.nightLight.applying
    readonly property bool scheduleEditable: root.controlsEnabled
                                             && root.nightLight.scheduleAvailable
    // AGENT-NOTE: Values mirror DisplayNightLightModel::ScheduleMode's
    // declaration order; the C++ enum is deliberately not referenced from QML
    // so this file also loads standalone in offscreen tests.
    readonly property int scheduleSunsetAuto: 0
    readonly property int scheduleSunsetManual: 1
    readonly property int scheduleCustomTimes: 2
    readonly property int scheduleAlways: 3
    readonly property var scheduleChoices: [
        {
            label: qsTr("Sunset to sunrise"),
            value: root.scheduleSunsetAuto,
            tip: qsTr("Follow the sun. The system may use this device's approximate location.")
        },
        {
            label: qsTr("Manual location"),
            value: root.scheduleSunsetManual,
            tip: qsTr("Follow the sun at coordinates you enter.")
        },
        {
            label: qsTr("Custom times"),
            value: root.scheduleCustomTimes,
            tip: qsTr("Warm the display between times you choose.")
        },
        {
            label: qsTr("Always on"),
            value: root.scheduleAlways,
            tip: qsTr("Keep the night temperature at all hours.")
        }
    ]

    spacing: Tokens.space["3"]

    // AGENT-GUARD: The slider preview must fire at most once per settled
    // value and must always be withdrawn when the drag ends without an
    // apply, or when the page closes: KWin holds a preview for 15 seconds
    // and the section never leaves one running behind the user.
    Timer {
        id: previewSettle

        interval: 250
        onTriggered: root.nightLight.previewTemperature(
            nightTemperatureRow.sliderValue)
    }

    function releasePreview() {
        previewSettle.stop()
        root.nightLight.stopPreview()
    }

    SectionHeader {
        objectName: "nightLightSectionHeader"
        Layout.fillWidth: true
        title: qsTr("Night light")
        description: root.nightLight.statusText
    }

    Label {
        objectName: "nightLightStatusLabel"
        Layout.fillWidth: true
        visible: root.nightLight.available
        text: root.nightLight.statusText
        font.pointSize: Tokens.type.caption
        muted: true
        textFormat: Text.PlainText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    DegradedNotice {
        objectName: "nightLightUnavailableNotice"
        Layout.fillWidth: true
        visible: !root.nightLight.available
        reason: qsTr("Night light is unavailable: the compositor's night light service did not answer.")
    }

    Label {
        objectName: "nightLightScheduleUnavailableLabel"
        Layout.fillWidth: true
        visible: root.nightLight.available && !root.nightLight.scheduleAvailable
        text: qsTr("Schedules are unavailable: the night light schedule service is not running. Temperatures still work.")
        font.pointSize: Tokens.type.caption
        muted: true
        textFormat: Text.PlainText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["3"]

        ShellIcons.Icon {
            objectName: "nightLightEnableIcon"
            name: "weather-clear-night"
            size: 22
            Layout.alignment: Qt.AlignVCenter
        }

        FormRow {
            objectName: "nightLightEnableRow"
            Layout.fillWidth: true
            label: qsTr("Night light")
            description: qsTr("Off or on right now.")
            editor: nightLightEnableSwitch

            Switch {
                id: nightLightEnableSwitch

                objectName: "nightLightEnableSwitch"
                enabled: root.controlsEnabled
                checked: root.nightLight.draftActive
                onToggled: root.nightLight.draftActive = checked
                accessibleDescription: qsTr("Turn night light off or on.")
                T.ToolTip.visible: hovered && root.controlsEnabled
                T.ToolTip.text: qsTr("When on, the display warms during the schedule below.")
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["3"]

        ShellIcons.Icon {
            objectName: "nightLightScheduleIcon"
            name: "preferences-system-time"
            size: 22
            Layout.alignment: Qt.AlignVCenter
        }

        FormRow {
            objectName: "nightLightScheduleRow"
            Layout.fillWidth: true
            label: qsTr("Schedule")
            description: root.nightLight.scheduleAvailable
                         ? qsTr("When the display warms.")
                         : qsTr("Schedule service is not running.")
            errorMessage: root.nightLight.scheduleAvailable ? ""
                          : qsTr("Schedule service unavailable")
            editor: nightLightScheduleCombo

            ComboBox {
                id: nightLightScheduleCombo

                objectName: "nightLightScheduleCombo"
                enabled: root.scheduleEditable
                model: root.scheduleChoices
                textRole: "label"
                currentIndex: root.nightLight.draftScheduleMode
                onActivated: function(index) {
                    root.nightLight.draftScheduleMode =
                        root.scheduleChoices[index].value
                }
                accessibleDescription: qsTr("Choose when the display warms.")
                T.ToolTip.visible: hovered && root.scheduleEditable
                T.ToolTip.text: root.scheduleChoices[currentIndex]?.tip ?? ""
            }
        }
    }

    DisplayNightLightLocationRow {
        visible: root.nightLight.draftScheduleMode
                 === root.scheduleSunsetManual
        nightLight: root.nightLight
        rowEnabled: root.scheduleEditable
    }

    DisplayNightLightTimesRows {
        visible: root.nightLight.draftScheduleMode
                 === root.scheduleCustomTimes
        nightLight: root.nightLight
        rowEnabled: root.scheduleEditable
    }

    DisplayNightLightTemperatureRow {
        id: nightTemperatureRow

        visible: true
        nightLight: root.nightLight
        rowEnabled: root.controlsEnabled
        rowIcon: "contrast-symbolic"
        rowObjectBase: "nightLightTemperature"
        rowLabel: qsTr("Night temperature")
        rowDescription: qsTr("Warmest color while night light is active.")
        sliderAccessibleDescription: qsTr(
            "Warmest color while night light is active, one thousand through sixty-five hundred kelvin.")
        kelvin: root.nightLight.draftNightTemperature
        onKelvinMoved: function(kelvin) {
            root.nightLight.draftNightTemperature = kelvin
            previewSettle.restart()
        }
        onWithdrawRequested: root.releasePreview()
    }

    DisplayNightLightTemperatureRow {
        nightLight: root.nightLight
        rowEnabled: root.controlsEnabled
        rowIcon: "weather-clear"
        rowObjectBase: "nightLightDayTemperature"
        rowLabel: qsTr("Day temperature")
        rowDescription: qsTr("Neutral color during the day.")
        sliderAccessibleDescription: qsTr(
            "Day color temperature, one thousand through sixty-five hundred kelvin.")
        kelvin: root.nightLight.draftDayTemperature
        onKelvinMoved: function(kelvin) {
            root.nightLight.draftDayTemperature = kelvin
        }
        onWithdrawRequested: root.releasePreview()
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.nightLight.draftDirty && root.nightLight.available
        spacing: Tokens.space["2"]

        Item {
            Layout.preferredWidth: 22
        }

        Button {
            objectName: "nightLightApplyButton"
            available: root.controlsEnabled
            emphasized: true
            text: qsTr("Apply")
            onClicked: root.nightLight.apply()
        }

        Button {
            objectName: "nightLightRevertButton"
            available: root.controlsEnabled
            text: qsTr("Revert")
            onClicked: root.nightLight.resetDraft()
        }

        Label {
            objectName: "nightLightValidationError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.nightLight.validationError
            font.pointSize: Tokens.type.caption
            color: Tokens.fg.default
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }

    Component.onDestruction: root.releasePreview()
}
