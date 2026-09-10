// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Item {
    id: root
    objectName: "weekView"

    required property var calendarController
    required property var occurrenceModel

    function weekDays() {
        const current = calendarController.currentDate
        const dow = current.getDay() === 0 ? 7 : current.getDay()
        const first = new Date(current.getFullYear(), current.getMonth(),
                               current.getDate() - ((dow - calendarController.weekStart + 7) % 7))
        const days = []
        for (let i = 0; i < 7; ++i) {
            const day = new Date(first.getFullYear(), first.getMonth(),
                                 first.getDate() + i)
            days.push({"date": day, "key": Qt.formatDate(day, "yyyy-MM-dd")})
        }
        return days
    }

    RowLayout {
        anchors.fill: parent
        spacing: Tokens.space["2"]

        Repeater {
            model: root.weekDays()

            delegate: ColumnLayout {
                id: dayColumn
                required property var modelData

                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Tokens.space["1"]

                Qinda.Label {
                    Layout.fillWidth: true
                    text: Qt.formatDate(modelData.date, "ddd d")
                    horizontalAlignment: Text.AlignHCenter
                }

                Repeater {
                    model: root.occurrenceModel.occurrencesForDay(modelData.key)

                    delegate: T.AbstractButton {
                        id: occurrenceEntry
                        required property var modelData

                        Layout.fillWidth: true
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.summary
                        onClicked: root.calendarController.selectEvent(modelData.eventUid)

                        contentItem: Qinda.Label {
                            text: (modelData.allDay
                                   ? modelData.summary
                                   : Qt.formatTime(modelData.start, "hh:mm")
                                     + " " + modelData.summary)
                            wrapMode: Text.WordWrap
                            Accessible.ignored: true
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
