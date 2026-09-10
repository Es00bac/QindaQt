// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Rectangle {
    id: root
    objectName: "calendarSidebar"

    required property var calendarController

    implicitWidth: 220
    color: Tokens.bg.raised

    function openNewCalendarDialog() {
        newCalendarNameField.text = ""
        newCalendarDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["3"]
        spacing: Tokens.space["2"]

        Qinda.SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Calendars")
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
                spacing: Tokens.space["2"]

                Qinda.CheckBox {
                    checked: calendarRow.modelData.enabled
                    text: calendarRow.modelData.displayName
                    accessibleDescription:
                        qsTr("Show or hide the %1 calendar")
                            .arg(calendarRow.modelData.displayName)
                    onToggled: root.calendarController.setCalendarEnabled(
                        calendarRow.modelData.id, checked)
                }
            }
        }

        Qinda.Button {
            id: newCalendarButton
            objectName: "newCalendarButton"
            Layout.fillWidth: true
            text: qsTr("New Calendar…")
            emphasized: false
            accessibleDescription: qsTr("Create a local calendar")
            onClicked: root.openNewCalendarDialog()
        }
    }

    T.Dialog {
        id: newCalendarDialog
        objectName: "newCalendarDialog"
        title: qsTr("New Calendar")
        modal: true
        standardButtons: T.Dialog.Ok | T.Dialog.Cancel
        anchors.centerIn: parent

        onAccepted: root.calendarController.createCalendar(newCalendarNameField.text)

        contentItem: ColumnLayout {
            spacing: Tokens.space["2"]

            Qinda.Label { text: qsTr("Name") }
            Qinda.TextField {
                id: newCalendarNameField
                Layout.fillWidth: true
                accessibleName: qsTr("Calendar name")
            }
        }
    }
}
