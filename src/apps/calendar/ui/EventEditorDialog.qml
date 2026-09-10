// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Create and edit event editor. Editing performs an in-place, uid-stable
// update through CalendarController.updateEvent (RFC 5545 revision bump);
// recurrence and reminder edits replace the stored rule/alarms wholesale.
Dialog {
    id: root
    objectName: "eventEditorDialog"

    required property var calendarController

    property alias summaryText: summaryField.text
    // Empty in create mode; the edited event's stable uid in edit mode.
    property string editingUid: ""
    property string calendarId: root.calendarController.defaultCalendarId
    property string startIso: ""
    property string endIso: ""
    property bool allDay: false
    property string locationText: ""
    property string descriptionText: ""
    property string recurrenceRule: ""
    property int reminderMinutes: -1

    readonly property var recurrenceRules: ["", "daily", "weekly", "monthly", "yearly"]
    readonly property var reminderChoices: [-1, 5, 10, 15, 30, 60]

    title: editingUid.length === 0 ? qsTr("New Event") : qsTr("Edit Event")
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    anchors.centerIn: parent

    function defaultStart() {
        const today = root.calendarController.currentDate
        return Qt.formatDate(today, "yyyy-MM-dd") + "T10:00:00"
    }

    function openForCreate() {
        root.editingUid = ""
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

    // Loads the controller's selectedEvent map; call only when an event is
    // selected. All-day end dates stay inclusive (KCalendarCore convention).
    function openForEdit() {
        const details = root.calendarController.selectedEvent
        if (!details.uid || details.uid.length === 0)
            return
        root.editingUid = details.uid
        summaryField.text = details.summary
        locationField.text = details.location
        descriptionField.text = details.description
        startField.text = details.startIso
        endField.text = details.endIso
        allDayCheck.checked = details.allDay
        recurrenceCombo.currentIndex = Math.max(0, root.recurrenceRules.indexOf(details.recurrenceRule))
        reminderCombo.currentIndex = Math.max(0, root.reminderChoices.indexOf(details.reminderMinutes))
        root.calendarId = details.calendarId
        const calendars = root.calendarController.calendars
        for (let i = 0; i < calendars.length; ++i) {
            if (calendars[i].id === details.calendarId) {
                calendarCombo.currentIndex = i
                break
            }
        }
        root.open()
    }

    onAccepted: {
        const calendars = root.calendarController.calendars
        const targetId = calendars.length > 0 && calendarCombo.currentIndex >= 0
                && calendarCombo.currentIndex < calendars.length
                ? calendars[calendarCombo.currentIndex].id
                : root.calendarController.defaultCalendarId
        if (root.editingUid.length === 0) {
            root.calendarController.createEvent(
                targetId, summaryField.text, startField.text, endField.text,
                allDayCheck.checked, locationField.text, descriptionField.text,
                root.recurrenceRule, root.reminderMinutes)
        } else {
            // The calendar is not movable in this milestone: the combo is
            // disabled in edit mode, so the stored calendar is kept.
            root.calendarController.updateEvent(
                root.editingUid, summaryField.text, startField.text, endField.text,
                allDayCheck.checked, locationField.text, descriptionField.text,
                root.recurrenceRule, root.reminderMinutes)
        }
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
            enabled: root.editingUid.length === 0
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
                root.recurrenceRule = root.recurrenceRules[currentIndex]
        }

        Label { text: qsTr("Reminder") }
        ComboBox {
            id: reminderCombo
            Layout.fillWidth: true
            Accessible.description: qsTr("Reminder before the event starts")
            model: [qsTr("None"), qsTr("5 minutes"), qsTr("10 minutes"),
                    qsTr("15 minutes"), qsTr("30 minutes"), qsTr("60 minutes")]
            onCurrentIndexChanged:
                root.reminderMinutes = root.reminderChoices[currentIndex]
        }
    }
}
