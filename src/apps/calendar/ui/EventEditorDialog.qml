// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

// Create-only event editor for Milestone 1. Editing an existing event is
// delete + recreate; recurrence/reminder editing after creation lands later.
T.Dialog {
    id: root
    objectName: "eventEditorDialog"

    required property var calendarController

    property alias summaryText: summaryField.text
    property string calendarId: root.calendarController.defaultCalendarId
    property string startIso: ""
    property string endIso: ""
    property bool allDay: false
    property string locationText: ""
    property string descriptionText: ""
    property string recurrenceRule: ""
    property int reminderMinutes: -1

    title: qsTr("New Event")
    modal: true
    standardButtons: T.Dialog.Ok | T.Dialog.Cancel
    anchors.centerIn: parent

    function defaultStart() {
        const today = root.calendarController.currentDate
        return Qt.formatDate(today, "yyyy-MM-dd") + "T10:00:00"
    }

    function openForCreate() {
        summaryField.text = ""
        locationField.text = ""
        descriptionField.text = ""
        startField.text = defaultStart()
        endField.text = Qt.formatDate(root.calendarController.currentDate, "yyyy-MM-dd") + "T11:00:00"
        allDayCheck.checked = false
        recurrenceCombo.currentIndex = 0
        reminderCombo.currentIndex = 0
        root.calendarId = root.calendarController.defaultCalendarId
        root.open()
    }

    onAccepted: {
        const calendars = root.calendarController.calendars
        const targetId = calendars.length > 0 && calendarCombo.currentIndex >= 0
                && calendarCombo.currentIndex < calendars.length
                ? calendars[calendarCombo.currentIndex].id
                : root.calendarController.defaultCalendarId
        root.calendarController.createEvent(
            targetId, summaryField.text, startField.text, endField.text,
            allDayCheck.checked, locationField.text, descriptionField.text,
            root.recurrenceRule, root.reminderMinutes)
    }

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        Qinda.Label { text: qsTr("Summary") }
        Qinda.TextField {
            id: summaryField
            Layout.fillWidth: true
            accessibleName: qsTr("Event summary")
            placeholderText: qsTr("Event title")
        }

        Qinda.Label { text: qsTr("Calendar") }
        Qinda.ComboBox {
            id: calendarCombo
            Layout.fillWidth: true
            accessibleDescription: qsTr("Calendar the event belongs to")
            model: root.calendarController.calendars
            textRole: "displayName"
        }

        Qinda.Label { text: qsTr("Start (yyyy-MM-ddTHH:mm)") }
        Qinda.TextField {
            id: startField
            Layout.fillWidth: true
            accessibleName: qsTr("Event start")
        }

        Qinda.Label { text: qsTr("End (yyyy-MM-ddTHH:mm)") }
        Qinda.TextField {
            id: endField
            Layout.fillWidth: true
            accessibleName: qsTr("Event end")
        }

        Qinda.CheckBox {
            id: allDayCheck
            text: qsTr("All day")
            accessibleDescription: qsTr("The event lasts whole days")
        }

        Qinda.Label { text: qsTr("Location") }
        Qinda.TextField {
            id: locationField
            Layout.fillWidth: true
            accessibleName: qsTr("Event location")
        }

        Qinda.Label { text: qsTr("Description") }
        Qinda.TextField {
            id: descriptionField
            Layout.fillWidth: true
            accessibleName: qsTr("Event description")
        }

        Qinda.Label { text: qsTr("Repeat") }
        Qinda.ComboBox {
            id: recurrenceCombo
            Layout.fillWidth: true
            accessibleDescription: qsTr("Recurrence rule")
            model: [qsTr("None"), qsTr("Daily"), qsTr("Weekly"),
                    qsTr("Monthly"), qsTr("Yearly")]
            onCurrentIndexChanged:
                root.recurrenceRule = ["", "daily", "weekly", "monthly", "yearly"][currentIndex]
        }

        Qinda.Label { text: qsTr("Reminder") }
        Qinda.ComboBox {
            id: reminderCombo
            Layout.fillWidth: true
            accessibleDescription: qsTr("Reminder before the event starts")
            model: [qsTr("None"), qsTr("5 minutes"), qsTr("10 minutes"),
                    qsTr("15 minutes"), qsTr("30 minutes"), qsTr("60 minutes")]
            onCurrentIndexChanged:
                root.reminderMinutes = [-1, 5, 10, 15, 30, 60][currentIndex]
        }
    }
}
