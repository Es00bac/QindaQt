// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "dayView"

    required property var calendarController
    required property var occurrenceModel

    readonly property string dayKey:
        Qt.formatDate(calendarController.currentDate, "yyyy-MM-dd")

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            Layout.fillWidth: true
            Layout.margins: 8
            text: Qt.formatDate(root.calendarController.currentDate,
                                "dddd, d MMMM yyyy")
            Accessible.name: text
        }

        ListView {
            id: dayList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.occurrenceModel.occurrencesForDay(root.dayKey)

            delegate: AbstractButton {
                id: occurrenceRow
                required property var modelData

                width: dayList.width
                Accessible.role: Accessible.Button
                Accessible.name: modelData.summary
                onClicked: root.calendarController.selectEvent(modelData.eventUid)

                contentItem: ColumnLayout {
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: modelData.allDay
                              ? qsTr("All day · %1").arg(modelData.summary)
                              : qsTr("%1 – %2 · %3")
                                .arg(Qt.formatTime(modelData.start, "hh:mm"))
                                .arg(Qt.formatTime(modelData.end, "hh:mm"))
                                .arg(modelData.summary)
                        wrapMode: Text.WordWrap
                        Accessible.ignored: true
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: modelData.location.length > 0
                        text: modelData.location
                        Accessible.ignored: true
                    }
                }
            }
        }
    }
}
