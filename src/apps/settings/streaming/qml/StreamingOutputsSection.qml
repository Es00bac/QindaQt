// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// What OBS is doing, and the four things the desktop can ask of it. Every
// control is disabled while there is no connection rather than hidden: the
// user is looking for these, and a missing row reads as a missing feature.
ColumnLayout {
    id: root

    required property var streamingSettings

    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Recording, streaming and the camera")
        description: qsTr("The same controls as the top-bar applet")
    }

    FormRow {
        objectName: "streamingRecordRow"
        Layout.fillWidth: true
        label: qsTr("Recording")
        description: root.streamingSettings.recording
                     ? qsTr("Recording for %1").arg(root.streamingSettings.recordingElapsed)
                     : qsTr("Save what is on screen to a file")
        editor: Switch {
            objectName: "streamingRecordSwitch"
            enabled: root.streamingSettings.connected
            checked: root.streamingSettings.recording
            accessibleDescription: qsTr("Whether OBS is recording")
            onToggled: root.streamingSettings.setRecording(checked)
        }
    }

    FormRow {
        objectName: "streamingStreamRow"
        Layout.fillWidth: true
        label: qsTr("Streaming")
        description: root.streamingSettings.streaming
                     ? qsTr("Streaming for %1").arg(root.streamingSettings.streamingElapsed)
                     : qsTr("Send what is on screen to the service configured in OBS")
        editor: Switch {
            objectName: "streamingStreamSwitch"
            enabled: root.streamingSettings.connected
            checked: root.streamingSettings.streaming
            accessibleDescription: qsTr("Whether OBS is streaming")
            onToggled: root.streamingSettings.setStreaming(checked)
        }
    }

    Label {
        objectName: "streamingDroppedWarning"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.streamingSettings.droppedFramesWarning
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    FormRow {
        objectName: "streamingVirtualCameraRow"
        Layout.fillWidth: true
        label: qsTr("Virtual camera")
        description: qsTr("Offer OBS's output as a camera to other applications")
        editor: Switch {
            objectName: "streamingVirtualCameraSwitch"
            enabled: root.streamingSettings.connected
            checked: root.streamingSettings.virtualCamera
            accessibleDescription: qsTr("Whether the virtual camera is running")
            onToggled: root.streamingSettings.setVirtualCamera(checked)
        }
    }

    FormRow {
        objectName: "streamingSceneRow"
        Layout.fillWidth: true
        visible: root.streamingSettings.sceneNames.length > 0
        label: qsTr("Scene")
        description: qsTr("Which of OBS's scenes is live")
        editor: ComboBox {
            id: sceneCombo
            objectName: "streamingSceneCombo"
            width: 320
            enabled: root.streamingSettings.connected
            model: root.streamingSettings.sceneNames
            currentIndex: root.streamingSettings.sceneNames.indexOf(
                              root.streamingSettings.currentScene)
            onActivated: index => {
                const names = root.streamingSettings.sceneNames
                if (index >= 0 && index < names.length)
                    root.streamingSettings.selectScene(names[index])
            }
        }
    }
}
