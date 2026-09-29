// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaTK as Tk

// Start and stop an OBS recording through the desktop's one OBS client.
//
// AGENT-CONTRACT: `recorder` (RecordController) decides every enablement and
// sentence, including the OBS applet's own unavailable reasons. State is
// always spelled out in text, never by colour alone.
Item {
    id: page

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Tk.Theme.space.md

        Tk.Heading {
            text: qsTr("Record the screen with OBS")
            Accessible.role: Accessible.Heading
        }

        Tk.Caption {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Records whatever the current OBS scene shows. Recording one region or one window is not available yet.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tk.Theme.space.sm
            Tk.Badge {
                objectName: "recordStateBadge"
                text: recorder.state === "paused" ? qsTr("Paused")
                      : recorder.recording ? qsTr("Recording") : qsTr("Idle")
                variant: recorder.recording ? "danger" : "default"
            }
            Tk.Label {
                objectName: "recordStatus"
                Layout.fillWidth: true
                // While OBS cannot be driven the notice below says why, once.
                visible: recorder.available
                wrapMode: Text.Wrap
                text: recorder.statusText
            }
            Tk.Mono {
                objectName: "recordElapsed"
                visible: recorder.recording
                text: recorder.elapsed
                Accessible.name: qsTr("Elapsed time %1").arg(recorder.elapsed)
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: Tk.Theme.space.sm
            Tk.Button {
                objectName: "recordToggle"
                variant: recorder.recording ? "danger" : "accent"
                iconName: recorder.recording ? "x" : "record"
                text: recorder.recording ? qsTr("Stop recording") : qsTr("Start recording")
                tooltip: qsTr("Start or stop recording (Meta+Alt+R anywhere)")
                enabled: recorder.canToggle
                busy: recorder.state === "starting" || recorder.state === "stopping"
                onClicked: recorder.toggle()
            }
            Tk.Button {
                objectName: "streamingSettings"
                iconName: "settings"
                text: qsTr("Streaming settings…")
                onClicked: screenshotApp.openStreamingSettings()
            }
        }

        Tk.Notice {
            objectName: "recordUnavailable"
            Layout.fillWidth: true
            visible: !recorder.available
            variant: "warning"
            text: recorder.statusText
            actions: Tk.Button {
                objectName: "recordOpenSettings"
                text: qsTr("Open Streaming settings")
                onClicked: screenshotApp.openStreamingSettings()
            }
        }

        Tk.Notice {
            objectName: "recordFeedback"
            Layout.fillWidth: true
            visible: recorder.feedback.length > 0
            variant: "danger"
            text: recorder.feedback
        }

        Tk.Card {
            objectName: "lastRecording"
            Layout.fillWidth: true
            visible: recorder.lastRecordingPath.length > 0
            implicitHeight: lastColumn.implicitHeight + 2 * Tk.Theme.space.md

            ColumnLayout {
                id: lastColumn
                anchors.fill: parent
                anchors.margins: Tk.Theme.space.md
                spacing: Tk.Theme.space.sm
                Tk.Label {
                    text: qsTr("Last recording")
                    font.weight: Font.DemiBold
                }
                Tk.Mono {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    text: recorder.lastRecordingPath
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: Tk.Theme.space.sm
                    Tk.Button {
                        iconName: "external-link"
                        text: qsTr("Open")
                        onClicked: screenshotApp.openRecording(recorder.lastRecordingPath)
                    }
                    Tk.Button {
                        iconName: "folder-open"
                        text: qsTr("Show in folder")
                        onClicked: screenshotApp.showRecordingInFolder(recorder.lastRecordingPath)
                    }
                    Tk.Button {
                        iconName: "copy"
                        text: qsTr("Copy path")
                        onClicked: screenshotApp.copyRecordingPath(recorder.lastRecordingPath)
                    }
                }
            }
        }
    }
}
