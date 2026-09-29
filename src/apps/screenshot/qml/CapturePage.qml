// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaTK as Tk

// What to capture and how. Options edit captureFlow directly; the defaults
// they start from are the Settings → Streaming capture preferences.
Item {
    id: page

    readonly property string modeHint: captureFlow.mode === "region"
        ? qsTr("Drag across any screen to choose the area. Arrow keys move it, Alt+arrows resize it, Enter captures and Esc cancels.")
        : captureFlow.mode === "window-under-pointer"
        ? qsTr("After the delay, click the window you want. Esc cancels.")
        : captureFlow.mode === "active-window"
        ? qsTr("Captures the window that was active before this one.")
        : qsTr("Captures without asking anything more.")

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Tk.Theme.space.md

        Tk.Heading {
            text: qsTr("Take a screenshot")
            Accessible.role: Accessible.Heading
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Tk.Theme.space.md
            rowSpacing: Tk.Theme.space.sm

            Tk.Label { text: qsTr("Area") }
            Tk.ComboBox {
                id: modeBox
                objectName: "modeBox"
                Layout.fillWidth: true
                model: captureFlow.modeChoices
                textRole: "text"
                valueRole: "id"
                // Not indexOfValue(): it is no binding dependency (see the
                // Streaming capture section's AGENT-GUARD).
                currentIndex: Math.max(0, captureFlow.modeChoices.findIndex(choice => choice.id === captureFlow.mode))
                Accessible.name: qsTr("Area to capture")
                onActivated: index => captureFlow.mode = captureFlow.modeChoices[index].id
            }

            Tk.Label { text: qsTr("Delay") }
            Tk.Segmented {
                objectName: "delayChoice"
                Layout.fillWidth: true
                stretch: true
                model: captureFlow.delayChoices.map(choice => choice.text)
                currentIndex: Math.max(0, captureFlow.delayChoices.findIndex(
                                  choice => choice.seconds === captureFlow.delaySeconds))
                Accessible.name: qsTr("Delay before capturing")
                onActivated: index => captureFlow.delaySeconds = captureFlow.delayChoices[index].seconds
            }
        }

        Tk.CheckBox {
            objectName: "includePointer"
            text: qsTr("Include the mouse pointer")
            checked: captureFlow.includePointer
            onToggled: captureFlow.includePointer = checked
        }
        Tk.CheckBox {
            objectName: "includeDecorations"
            text: qsTr("Include the window title bar and borders")
            enabled: captureFlow.windowMode
            checked: captureFlow.includeDecorations
            onToggled: captureFlow.includeDecorations = checked
        }

        Tk.Caption {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: page.modeHint
        }

        Flow {
            Layout.fillWidth: true
            spacing: Tk.Theme.space.sm
            Tk.Button {
                objectName: "takeScreenshot"
                variant: "accent"
                iconName: "camera"
                text: qsTr("Take screenshot")
                tooltip: qsTr("Take a screenshot (Ctrl+N)")
                enabled: captureFlow.phase === "idle"
                onClicked: captureFlow.start()
            }
            Tk.Button {
                objectName: "capturePreferences"
                iconName: "settings"
                text: qsTr("Preferences…")
                tooltip: qsTr("Save folder, file names and defaults in Settings")
                onClicked: screenshotApp.openCaptureSettings()
            }
        }

        Tk.Notice {
            objectName: "captureNotice"
            Layout.fillWidth: true
            visible: screenshotApp.resultMessage.length > 0
            variant: "danger"
            text: screenshotApp.resultMessage
        }

        Tk.Caption {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Anywhere on the desktop: Print captures a region, Shift+Print every screen and Alt+Print the active window. Change these in Settings → Input → Shortcuts.")
        }
    }
}
