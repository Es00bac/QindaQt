// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    objectName: "calendarSidebar"

    required property var calendarController

    implicitWidth: 220
    padding: 8

    function openNewCalendarDialog() {
        newCalendarNameField.text = ""
        newCalendarDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            Layout.fillWidth: true
            text: qsTr("Calendars")
            font.bold: true
            Accessible.name: text
        }

        ListView {
            id: calendarList
            objectName: "calendarList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.calendarController.calendars

            delegate: RowLayout {
                id: calendarRow
                required property var modelData

                width: calendarList.width
                spacing: 4

                CheckBox {
                    checked: calendarRow.modelData.enabled
                    text: calendarRow.modelData.displayName
                    Accessible.description:
                        qsTr("Show or hide the %1 calendar")
                            .arg(calendarRow.modelData.displayName)
                    onToggled: root.calendarController.setCalendarEnabled(
                        calendarRow.modelData.id, checked)
                }
            }
        }

        Button {
            id: newCalendarButton
            objectName: "newCalendarButton"
            Layout.fillWidth: true
            text: qsTr("New Calendar…")
            Accessible.description: qsTr("Create a local calendar")
            onClicked: root.openNewCalendarDialog()
        }
    }

    Dialog {
        id: newCalendarDialog
        objectName: "newCalendarDialog"
        title: qsTr("New Calendar")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent

        onAccepted: root.calendarController.createCalendar(newCalendarNameField.text)

        contentItem: ColumnLayout {
            spacing: 4

            Label { text: qsTr("Name") }
            TextField {
                id: newCalendarNameField
                Layout.fillWidth: true
                Accessible.name: qsTr("Calendar name")
            }
        }
    }
}
