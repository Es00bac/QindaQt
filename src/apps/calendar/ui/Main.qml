// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ADR-0116: stock Qt Quick Controls only — no QindaQt.Tokens/Controls imports
// and no palette literals; appearance comes from the Qt platform theme.
// The AppShell seams stay non-visual: the window binds the injected
// ApplicationCoordinator for the action catalog, in-window menus, quit
// arbitration, focus reporting, and the degraded-integration notice.
ApplicationWindow {
    id: root

    required property var coordinator
    required property var calendarController
    required property var occurrenceModel

    property bool closeAuthorized: false
    property bool inWindowMenuVisible: true

    visible: true
    width: 1024
    height: 640
    minimumWidth: 560
    minimumHeight: 360
    title: coordinator.windowTitle.length > 0
           ? coordinator.windowTitle : coordinator.applicationName

    // AGENT-CONTRACT: Closing asks the owning application for a decision. The
    // coordinator and this surface never call QCoreApplication::quit or infer
    // whether domain state is safe to discard.
    onClosing: function(close) {
        if (closeAuthorized) {
            close.accepted = true
            return
        }
        close.accepted = false
        coordinator.requestQuit("window-close")
    }

    onActiveFocusItemChanged: {
        const owner = activeFocusItem && activeFocusItem.objectName
                    ? activeFocusItem.objectName : ""
        coordinator.reportFocusOwner(owner)
    }

    Component.onCompleted: {
        if (coordinator.initialFocusObjectName.length === 0
                || coordinator.initialFocusObjectName === newEventButton.objectName)
            newEventButton.forceActiveFocus(Qt.TabFocusReason)
    }

    Connections {
        target: root.coordinator
        function onQuitApproved(requestId) {
            root.closeAuthorized = true
            root.close()
        }
    }

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

    // In-window menu authority, hidden when the global-menu export claims the
    // window (composeCalendarMenuExport flips inWindowMenuVisible).
    menuBar: MenuBar {
        id: exportedMenuBar
        objectName: "appShellMenuBar"
        visible: root.inWindowMenuVisible

        Instantiator {
            model: root.coordinator.menus

            delegate: Menu {
                id: exportedMenu
                required property var modelData
                title: modelData.label

                Instantiator {
                    model: exportedMenu.modelData.actions

                    delegate: Action {
                        required property var modelData
                        text: modelData.label
                        enabled: modelData.enabled
                        checkable: modelData.checkable
                        checked: modelData.checked
                        shortcut: modelData.shortcut
                        onTriggered: root.coordinator.activateAction(modelData.id)
                    }

                    onObjectAdded: function(index, object) {
                        exportedMenu.insertAction(index, object)
                    }
                    onObjectRemoved: function(index, object) {
                        exportedMenu.removeAction(object)
                    }
                }
            }

            onObjectAdded: function(index, object) {
                exportedMenuBar.insertMenu(index, object)
            }
            onObjectRemoved: function(index, object) {
                exportedMenuBar.removeMenu(object)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        RowLayout {
            id: toolbar
            Layout.fillWidth: true
            spacing: 4

            Button {
                id: newEventButton
                objectName: "newEventButton"
                text: qsTr("New Event")
                Accessible.description: qsTr("Create an event in the default calendar")
                onClicked: eventEditor.openForCreate()
            }
            Button {
                id: todayButton
                objectName: "todayButton"
                text: qsTr("Today")
                Accessible.description: qsTr("Return the view to the current date")
                onClicked: root.calendarController.goToday()
            }
            Button {
                id: previousPeriodButton
                objectName: "previousPeriodButton"
                text: qsTr("◀")
                Accessible.description: qsTr("Show the previous month, week, or day")
                onClicked: root.calendarController.previousPeriod()
            }
            Button {
                id: nextPeriodButton
                objectName: "nextPeriodButton"
                text: qsTr("▶")
                Accessible.description: qsTr("Show the next month, week, or day")
                onClicked: root.calendarController.nextPeriod()
            }
            Label {
                text: root.calendarController.periodTitle
                Accessible.name: root.calendarController.periodTitle
            }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "viewMonthButton"
                text: qsTr("Month")
                checkable: true
                checked: root.calendarController.viewMode === "month"
                Accessible.description: qsTr("Show one month of events")
                onClicked: root.calendarController.setViewMode("month")
            }
            Button {
                objectName: "viewWeekButton"
                text: qsTr("Week")
                checkable: true
                checked: root.calendarController.viewMode === "week"
                Accessible.description: qsTr("Show one week of events")
                onClicked: root.calendarController.setViewMode("week")
            }
            Button {
                objectName: "viewDayButton"
                text: qsTr("Day")
                checkable: true
                checked: root.calendarController.viewMode === "day"
                Accessible.description: qsTr("Show one day of events")
                onClicked: root.calendarController.setViewMode("day")
            }
        }

        // Degraded AppShell integrations remain usable; keep the notice's
        // title distinct for an unavailable integration.
        Rectangle {
            objectName: "appShellDegradedNotice"
            Layout.fillWidth: true
            visible: root.coordinator.degraded
            implicitHeight: degradedLabel.implicitHeight + 12
            radius: 4
            color: root.palette.alternateBase
            border.color: root.palette.mid
            Accessible.role: Accessible.AlertMessage
            Accessible.name: degradedLabel.text

            Label {
                id: degradedLabel
                anchors.fill: parent
                anchors.margins: 6
                wrapMode: Text.WordWrap
                text: (root.coordinator.hasUnavailableIntegration
                       ? qsTr("Feature unavailable") : qsTr("Limited capability"))
                      + " — " + root.coordinator.degradedMessage
            }
        }

        // Fallback when the D-Bus reminder delivery fails (reminder_delivery).
        Rectangle {
            id: reminderBanner
            objectName: "reminderBanner"
            Layout.fillWidth: true
            visible: root.calendarController.reminderBannerText.length > 0
            implicitHeight: reminderRow.implicitHeight + 12
            radius: 4
            color: root.palette.alternateBase
            border.color: root.palette.mid
            Accessible.role: Accessible.AlertMessage
            Accessible.name: qsTr("Reminder: %1").arg(root.calendarController.reminderBannerText)

            RowLayout {
                id: reminderRow
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Reminder") + " — " + root.calendarController.reminderBannerText
                    Accessible.ignored: true
                }
                Button {
                    text: qsTr("Dismiss")
                    Accessible.description: qsTr("Dismiss the reminder")
                    onClicked: root.calendarController.dismissReminderBanner()
                }
            }
        }

        Rectangle {
            objectName: "calendarLoadErrorCard"
            Layout.fillWidth: true
            visible: root.calendarController.loadError.length > 0
            implicitHeight: loadErrorLabel.implicitHeight + 12
            radius: 4
            color: root.palette.alternateBase
            border.color: root.palette.mid
            Accessible.role: Accessible.AlertMessage
            Accessible.name: loadErrorLabel.text

            Label {
                id: loadErrorLabel
                anchors.fill: parent
                anchors.margins: 6
                wrapMode: Text.WordWrap
                text: qsTr("Calendar storage problem") + " — " + root.calendarController.loadError
            }
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

                MonthViewGrid {
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
