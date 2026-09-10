// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// AGENT-NOTE: the month component is named MonthViewGrid, not MonthGrid —
// QtQuick.Controls.Basic ships its own MonthGrid control and an unqualified
// lookup resolves to it (silently breaking property assignment).
Item {
    id: root
    objectName: "monthView"

    required property var calendarController
    required property var occurrenceModel

    readonly property date currentDate: calendarController.currentDate
    readonly property int weekStart: calendarController.weekStart

    function dayKey(date) {
        return Qt.formatDate(date, "yyyy-MM-dd")
    }

    // Day-of-week labels ordered from weekStart.
    function weekdayNames() {
        const names = []
        for (let i = 0; i < 7; ++i) {
            const day = ((root.weekStart - 1 + i) % 7) + 1
            names.push(Qt.locale().dayName(day, Locale.ShortFormat))
        }
        return names
    }

    // 42 cells (6 rows x 7) starting on the weekStart of the first week shown.
    function monthCells() {
        const first = new Date(root.currentDate.getFullYear(),
                               root.currentDate.getMonth(), 1)
        const firstDow = first.getDay() === 0 ? 7 : first.getDay()
        const lead = (firstDow - root.weekStart + 7) % 7
        const cells = []
        for (let i = 0; i < 42; ++i) {
            const day = new Date(first.getFullYear(), first.getMonth(),
                                 1 + i - lead)
            cells.push({
                "date": day,
                "inMonth": day.getMonth() === root.currentDate.getMonth(),
                "key": root.dayKey(day)
            })
        }
        return cells
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        GridLayout {
            Layout.fillWidth: true
            columns: 7
            columnSpacing: 0

            Repeater {
                model: root.weekdayNames()
                Label {
                    required property string modelData
                    Layout.fillWidth: true
                    text: modelData
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 7
            rowSpacing: 0
            columnSpacing: 0

            Repeater {
                model: root.monthCells()

                delegate: AbstractButton {
                    id: dayCell
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Accessible.role: Accessible.Button
                    Accessible.name: Qt.formatDate(modelData.date, "dddd, d MMMM yyyy")

                    onClicked: {
                        root.calendarController.goToDate(modelData.key)
                        root.calendarController.setViewMode("day")
                    }

                    contentItem: ColumnLayout {
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: modelData.date.getDate()
                            opacity: modelData.inMonth ? 1.0 : 0.4
                            Accessible.ignored: true
                        }

                        Repeater {
                            model: root.occurrenceModel.occurrencesForDay(modelData.key)

                            delegate: AbstractButton {
                                id: occurrenceChip
                                required property var modelData

                                Layout.fillWidth: true
                                Accessible.role: Accessible.Button
                                Accessible.name: modelData.summary
                                onClicked: root.calendarController.selectEvent(modelData.eventUid)

                                contentItem: Label {
                                    text: (modelData.allDay
                                           ? modelData.summary
                                           : Qt.formatTime(modelData.start, "hh:mm")
                                             + " " + modelData.summary)
                                    elide: Text.ElideRight
                                    Accessible.ignored: true
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }
}
