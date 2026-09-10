// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Details pane for the selected event. Edit performs a uid-stable in-place
// update (the dialog lives in Main; this pane only requests it), Delete
// removes the event through the controller; both persist atomically.
Pane {
    id: root
    objectName: "eventDetailsPane"

    required property var calendarController
    signal editRequested()

    implicitWidth: 280
    padding: 8
    visible: root.calendarController.selectedEventUid.length > 0
             && root.calendarController.selectedEvent.summary !== undefined

    function recurrenceText(rule) {
        if (rule === "daily") return qsTr("Daily")
        if (rule === "weekly") return qsTr("Weekly")
        if (rule === "monthly") return qsTr("Monthly")
        if (rule === "yearly") return qsTr("Yearly")
        return qsTr("Does not repeat")
    }

    function whenText(details) {
        const start = new Date(details.startIso)
        const end = new Date(details.endIso)
        if (details.allDay) {
            const sameDay = details.startIso.slice(0, 10) === details.endIso.slice(0, 10)
            return sameDay
                ? qsTr("All day · %1").arg(Qt.formatDate(start, "dddd, d MMMM yyyy"))
                : qsTr("All day · %1 – %2")
                    .arg(Qt.formatDate(start, "d MMMM yyyy"))
                    .arg(Qt.formatDate(end, "d MMMM yyyy"))
        }
        return qsTr("%1 %2 – %3")
            .arg(Qt.formatDate(start, "d MMMM yyyy"))
            .arg(Qt.formatTime(start, "hh:mm"))
            .arg(Qt.formatTime(end, "hh:mm"))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6
        visible: root.visible

        Label {
            Layout.fillWidth: true
            text: root.calendarController.selectedEvent.summary ?? ""
            font.bold: true
            wrapMode: Text.WordWrap
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            text: root.whenText(root.calendarController.selectedEvent)
            wrapMode: Text.WordWrap
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Calendar: %1").arg(root.calendarController.selectedEvent.calendarName ?? "")
            wrapMode: Text.WordWrap
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Repeat: %1").arg(root.recurrenceText(root.calendarController.selectedEvent.recurrenceRule ?? ""))
            wrapMode: Text.WordWrap
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            visible: (root.calendarController.selectedEvent.reminderMinutes ?? -1) >= 0
            text: qsTr("Reminder: %1 minutes before").arg(root.calendarController.selectedEvent.reminderMinutes)
            wrapMode: Text.WordWrap
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            visible: (root.calendarController.selectedEvent.location ?? "").length > 0
            text: root.calendarController.selectedEvent.location ?? ""
            wrapMode: Text.WordWrap
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: (root.calendarController.selectedEvent.description ?? "").length > 0
            text: root.calendarController.selectedEvent.description ?? ""
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignTop
            Accessible.ignored: true
        }
        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Button {
                objectName: "editEventButton"
                Layout.fillWidth: true
                text: qsTr("Edit…")
                Accessible.description: qsTr("Edit the selected event")
                onClicked: root.editRequested()
            }
            Button {
                objectName: "deleteEventButton"
                Layout.fillWidth: true
                text: qsTr("Delete")
                Accessible.description: qsTr("Delete the selected event from its calendar")
                onClicked: root.calendarController.deleteEvent(
                    root.calendarController.selectedEventUid)
            }
        }
    }
}
