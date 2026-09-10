// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Create-only event editor for Milestone 1. Editing an existing event is
// delete + recreate; recurrence/reminder editing after creation lands later.
Dialog {
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
    standardButtons: Dialog.Ok | Dialog.Cancel
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
        spacing: 4

        Label { text: qsTr("Summary") }
        TextField {
            id: summaryField
            Layout.fillWidth: true
            Accessible.name: qsTr("Event summary")
            placeholderText: qsTr("Event title")
        }

        Label { text: qsTr("Calendar") }
        ComboBox {
            id: calendarCombo
            Layout.fillWidth: true
            Accessible.description: qsTr("Calendar the event belongs to")
            model: root.calendarController.calendars
            textRole: "displayName"
        }

        Label { text: qsTr("Start (yyyy-MM-ddTHH:mm)") }
        TextField {
            id: startField
            Layout.fillWidth: true
            Accessible.name: qsTr("Event start")
        }

        Label { text: qsTr("End (yyyy-MM-ddTHH:mm)") }
        TextField {
            id: endField
            Layout.fillWidth: true
            Accessible.name: qsTr("Event end")
        }

        CheckBox {
            id: allDayCheck
            text: qsTr("All day")
            Accessible.description: qsTr("The event lasts whole days")
        }

        Label { text: qsTr("Location") }
        TextField {
            id: locationField
            Layout.fillWidth: true
            Accessible.name: qsTr("Event location")
        }

        Label { text: qsTr("Description") }
        TextField {
            id: descriptionField
            Layout.fillWidth: true
            Accessible.name: qsTr("Event description")
        }

        Label { text: qsTr("Repeat") }
        ComboBox {
            id: recurrenceCombo
            Layout.fillWidth: true
            Accessible.description: qsTr("Recurrence rule")
            model: [qsTr("None"), qsTr("Daily"), qsTr("Weekly"),
                    qsTr("Monthly"), qsTr("Yearly")]
            onCurrentIndexChanged:
                root.recurrenceRule = ["", "daily", "weekly", "monthly", "yearly"][currentIndex]
        }

        Label { text: qsTr("Reminder") }
        ComboBox {
            id: reminderCombo
            Layout.fillWidth: true
            Accessible.description: qsTr("Reminder before the event starts")
            model: [qsTr("None"), qsTr("5 minutes"), qsTr("10 minutes"),
                    qsTr("15 minutes"), qsTr("30 minutes"), qsTr("60 minutes")]
            onCurrentIndexChanged:
                root.reminderMinutes = [-1, 5, 10, 15, 30, 60][currentIndex]
        }
    }
}
