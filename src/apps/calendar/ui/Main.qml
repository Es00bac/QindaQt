// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.AppShell 1.0
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

ApplicationShell {
    id: root

    required property var calendarController
    required property var occurrenceModel

    initialFocusItem: newEventButton
    width: 1024
    height: 640
    minimumWidth: 560
    minimumHeight: 360

    Connections {
        target: root.coordinator
        function onActionRequested(actionId) {
            const controller = root.calendarController
            if (actionId === "event.new") {
                eventEditor.openForCreate()
            } else if (actionId === "file.import-ics") {
                importExport.openImport()
            } else if (actionId === "file.export-ics") {
                importExport.openExport()
            } else if (actionId === "event.delete") {
                if (controller.selectedEventUid.length > 0)
                    controller.deleteEvent(controller.selectedEventUid)
            } else if (actionId === "view.month") {
                controller.setViewMode("month")
            } else if (actionId === "view.week") {
                controller.setViewMode("week")
            } else if (actionId === "view.day") {
                controller.setViewMode("day")
            } else if (actionId === "go.today") {
                controller.goToday()
            } else if (actionId === "go.previous-period") {
                controller.previousPeriod()
            } else if (actionId === "go.next-period") {
                controller.nextPeriod()
            } else if (actionId === "calendar.new") {
                sidebar.openNewCalendarDialog()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            id: toolbar
            Layout.fillWidth: true
            Layout.margins: Tokens.space["3"]
            spacing: Tokens.space["2"]

            Qinda.Button {
                id: newEventButton
                objectName: "newEventButton"
                text: qsTr("New Event")
                accessibleDescription: qsTr("Create an event in the default calendar")
                onClicked: eventEditor.openForCreate()
            }
            Qinda.Button {
                id: todayButton
                objectName: "todayButton"
                text: qsTr("Today")
                emphasized: false
                accessibleDescription: qsTr("Return the view to the current date")
                onClicked: root.calendarController.goToday()
            }
            Qinda.Button {
                id: previousPeriodButton
                objectName: "previousPeriodButton"
                text: qsTr("◀")
                emphasized: false
                accessibleDescription: qsTr("Show the previous month, week, or day")
                onClicked: root.calendarController.previousPeriod()
            }
            Qinda.Button {
                id: nextPeriodButton
                objectName: "nextPeriodButton"
                text: qsTr("▶")
                emphasized: false
                accessibleDescription: qsTr("Show the next month, week, or day")
                onClicked: root.calendarController.nextPeriod()
            }
            Qinda.Label {
                text: root.calendarController.periodTitle
                Accessible.name: root.calendarController.periodTitle
            }
            Item { Layout.fillWidth: true }
            Qinda.Button {
                objectName: "viewMonthButton"
                text: qsTr("Month")
                emphasized: false
                checkable: true
                checked: root.calendarController.viewMode === "month"
                accessibleDescription: qsTr("Show one month of events")
                onClicked: root.calendarController.setViewMode("month")
            }
            Qinda.Button {
                objectName: "viewWeekButton"
                text: qsTr("Week")
                emphasized: false
                checkable: true
                checked: root.calendarController.viewMode === "week"
                accessibleDescription: qsTr("Show one week of events")
                onClicked: root.calendarController.setViewMode("week")
            }
            Qinda.Button {
                objectName: "viewDayButton"
                text: qsTr("Day")
                emphasized: false
                checkable: true
                checked: root.calendarController.viewMode === "day"
                accessibleDescription: qsTr("Show one day of events")
                onClicked: root.calendarController.setViewMode("day")
            }
        }

        Qinda.StateCard {
            id: reminderBanner
            objectName: "reminderBanner"
            Layout.fillWidth: true
            visible: root.calendarController.reminderBannerText.length > 0
            status: Qinda.StateCard.Information
            title: qsTr("Reminder")
            message: root.calendarController.reminderBannerText
            actionText: qsTr("Dismiss")
            onActionTriggered: root.calendarController.dismissReminderBanner()
        }

        Qinda.StateCard {
            objectName: "calendarLoadErrorCard"
            Layout.fillWidth: true
            visible: root.calendarController.loadError.length > 0
            status: Qinda.StateCard.Error
            title: qsTr("Calendar storage problem")
            message: root.calendarController.loadError
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            CalendarSidebar {
                id: sidebar
                Layout.fillHeight: true
                calendarController: root.calendarController
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.calendarController.viewMode === "week" ? 1
                            : root.calendarController.viewMode === "day" ? 2 : 0

                MonthGrid {
                    calendarController: root.calendarController
                    occurrenceModel: root.occurrenceModel
                }
                WeekView {
                    calendarController: root.calendarController
                    occurrenceModel: root.occurrenceModel
                }
                DayView {
                    calendarController: root.calendarController
                    occurrenceModel: root.occurrenceModel
                }
            }
        }
    }

    EventEditorDialog {
        id: eventEditor
        calendarController: root.calendarController
    }

    ImportExportDialog {
        id: importExport
        calendarController: root.calendarController
    }
}
