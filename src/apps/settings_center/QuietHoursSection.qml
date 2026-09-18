// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// ADR-0212: the Do Not Disturb schedule. This section publishes only what the
// settings service last confirmed -- a refused or silently-ignored edit puts
// every control straight back where the model still is, so the page can never
// claim a quiet window the machine does not have.
ColumnLayout {
    id: section

    required property var schedule
    // The page owns the focus ring; this section joins it at both ends.
    property Item focusBefore: null
    property Item focusAfter: null
    readonly property Item firstControl: scheduleSwitch
    readonly property Item lastControl: endField

    spacing: Tokens.space["3"]

    Switch {
        id: scheduleSwitch
        objectName: "settingsQuietHoursSwitch"
        text: qsTr("Quiet on a schedule")
        checked: section.schedule.scheduleEnabled
        enabled: section.schedule.available
        focusPolicy: Qt.StrongFocus
        KeyNavigation.tab: startField
        KeyNavigation.backtab: section.focusBefore
        Accessible.role: Accessible.CheckBox
        Accessible.name: qsTr("Quiet notifications on a schedule")
        Accessible.description: section.schedule.summaryText
        Accessible.checked: checked
        onClicked: section.schedule.setScheduleEnabled(
                       !section.schedule.scheduleEnabled)
    }

    RowLayout {
        objectName: "settingsQuietHoursTimes"
        Layout.fillWidth: true
        spacing: Tokens.space["3"]
        // The times stay readable while the schedule is off, so a user can
        // see what turning it on would do before they do it.
        enabled: section.schedule.available

        Label {
            text: qsTr("From")
            font.family: Tokens.type.fontFamily
            textFormat: Text.PlainText
            Accessible.ignored: true
        }
        TextField {
            id: startField
            objectName: "settingsQuietHoursStart"
            implicitWidth: 96
            text: section.schedule.startText
            inputMask: "99:99"
            KeyNavigation.tab: endField
            KeyNavigation.backtab: scheduleSwitch
            Accessible.name: qsTr("Quiet hours start, 24-hour")
            Accessible.description: section.schedule.summaryText
            // Committed on Enter or focus loss, never per keystroke: a
            // half-typed hour must not become the time the machine quiets.
            // AGENT-GUARD: the mask admits 99:99, which is not a time. The
            // model refuses it in silence, so this rebind is the only thing
            // that puts the field back -- do not drop it.
            onEditingFinished: {
                const parts = text.split(":")
                section.schedule.setStart(Number(parts[0]), Number(parts[1]))
                text = Qt.binding(() => section.schedule.startText)
            }
        }
        Label {
            text: qsTr("until")
            font.family: Tokens.type.fontFamily
            textFormat: Text.PlainText
            Accessible.ignored: true
        }
        TextField {
            id: endField
            objectName: "settingsQuietHoursEnd"
            implicitWidth: 96
            text: section.schedule.endText
            inputMask: "99:99"
            KeyNavigation.tab: section.focusAfter
            KeyNavigation.backtab: startField
            Accessible.name: qsTr("Quiet hours end, 24-hour")
            Accessible.description: section.schedule.summaryText
            onEditingFinished: {
                const parts = text.split(":")
                section.schedule.setEnd(Number(parts[0]), Number(parts[1]))
                text = Qt.binding(() => section.schedule.endText)
            }
        }
        Item { Layout.fillWidth: true }
    }

    Label {
        objectName: "settingsQuietHoursSummary"
        Layout.fillWidth: true
        text: section.schedule.summaryText
        font.family: Tokens.type.fontFamily
        color: Tokens.fg.muted
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Label {
        objectName: "settingsQuietHoursError"
        Layout.fillWidth: true
        visible: section.schedule.errorText.length > 0
        text: section.schedule.errorText
        font.family: Tokens.type.fontFamily
        color: Tokens.danger.default
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
