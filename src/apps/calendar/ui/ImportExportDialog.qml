// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Milestone 1 keeps path entry as plain text fields; portal file chooser
// integration is a documented follow-up.
Item {
    id: root

    required property var calendarController

    function openImport() {
        importPathField.text = ""
        importResultLabel.text = ""
        importDialog.open()
    }

    function openExport() {
        exportPathField.text = ""
        exportResultLabel.text = ""
        exportDialog.open()
    }

    Dialog {
        id: importDialog
        objectName: "importDialog"
        title: qsTr("Import Calendar")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent

        onAccepted: {
            const calendars = root.calendarController.calendars
            const targetId = calendars.length > 0 && importCalendarCombo.currentIndex >= 0
                    && importCalendarCombo.currentIndex < calendars.length
                    ? calendars[importCalendarCombo.currentIndex].id
                    : root.calendarController.defaultCalendarId
            const result = root.calendarController.importIcs(
                importPathField.text, targetId)
            importResultLabel.text = result.ok
                ? qsTr("Imported %1 events (%2 duplicates skipped)")
                    .arg(result.imported).arg(result.skippedDuplicates)
                : qsTr("Import failed: %1").arg(result.error)
        }

        contentItem: ColumnLayout {
            spacing: 4

            Label { text: qsTr("File path (.ics)") }
            TextField {
                id: importPathField
                Layout.fillWidth: true
                Accessible.name: qsTr("iCalendar file to import")
            }

            Label { text: qsTr("Into calendar") }
            ComboBox {
                id: importCalendarCombo
                Layout.fillWidth: true
                Accessible.description: qsTr("Target calendar for imported events")
                model: root.calendarController.calendars
                textRole: "displayName"
            }

            Label {
                id: importResultLabel
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: exportDialog
        objectName: "exportDialog"
        title: qsTr("Export Calendar")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent

        onAccepted: {
            const calendars = root.calendarController.calendars
            const allCalendars = exportCalendarCombo.currentIndex === 0
            const targetId = allCalendars
                || exportCalendarCombo.currentIndex < 1
                || exportCalendarCombo.currentIndex > calendars.length
                ? "" : calendars[exportCalendarCombo.currentIndex - 1].id
            const result = root.calendarController.exportIcs(
                exportPathField.text, targetId)
            exportResultLabel.text = result.ok
                ? qsTr("Exported %1 events").arg(result.exported)
                : qsTr("Export failed: %1").arg(result.error)
        }

        contentItem: ColumnLayout {
            spacing: 4

            Label { text: qsTr("File path (.ics)") }
            TextField {
                id: exportPathField
                Layout.fillWidth: true
                Accessible.name: qsTr("Destination iCalendar file")
            }

            Label { text: qsTr("Calendar") }
            ComboBox {
                id: exportCalendarCombo
                Layout.fillWidth: true
                Accessible.description: qsTr("Calendar to export")
                model: {
                    const entries = [qsTr("All calendars")]
                    const calendars = root.calendarController.calendars
                    for (let i = 0; i < calendars.length; ++i)
                        entries.push(calendars[i].displayName)
                    return entries
                }
            }

            Label {
                id: exportResultLabel
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }
}
