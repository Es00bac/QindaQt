// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The OBS applet's popup contents: what OBS is doing, the things the desktop
// can ask of it, the scene list, and a way into OBS itself.
//
// AGENT-CONTRACT: a control here is enabled exactly when the controller says
// it is dispatchable. Nothing in this popup pretends to work while OBS is
// closed.
ColumnLayout {
    id: root

    required property var controller

    spacing: Tokens.space["2"]

    Label {
        objectName: "obsAppletUnavailable"
        Layout.fillWidth: true
        visible: text.length > 0
        muted: true
        text: root.controller.unavailableText
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    Label {
        objectName: "obsAppletFeedback"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.controller.feedback
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    Label {
        objectName: "obsAppletDroppedWarning"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.controller.droppedFramesWarning
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    ObsAppletToggleRow {
        objectName: "obsAppletRecordRow"
        Layout.fillWidth: true
        label: qsTr("Recording")
        elapsed: root.controller.recordingElapsed
        active: root.controller.recording
        available: root.controller.controlAvailable
        accessibleDescription: qsTr("Whether OBS is recording")
        onToggled: root.controller.toggleRecording()
    }

    ObsAppletToggleRow {
        objectName: "obsAppletStreamRow"
        Layout.fillWidth: true
        label: qsTr("Streaming")
        elapsed: root.controller.streamingElapsed
        active: root.controller.streaming
        available: root.controller.controlAvailable
        accessibleDescription: qsTr("Whether OBS is streaming")
        onToggled: root.controller.toggleStreaming()
    }

    ObsAppletToggleRow {
        objectName: "obsAppletVirtualCameraRow"
        Layout.fillWidth: true
        label: qsTr("Virtual camera")
        active: root.controller.virtualCamera
        available: root.controller.controlAvailable
        accessibleDescription: qsTr("Whether the virtual camera is running")
        onToggled: root.controller.toggleVirtualCamera()
    }

    Label {
        objectName: "obsAppletSceneHeading"
        Layout.fillWidth: true
        visible: root.controller.sceneNames.length > 0
        muted: true
        text: qsTr("Scene")
    }

    Repeater {
        model: root.controller.sceneNames

        delegate: Button {
            id: sceneButton
            required property string modelData
            objectName: "obsAppletScene_" + modelData
            Layout.fillWidth: true
            text: modelData
            emphasized: modelData === root.controller.currentScene
            available: root.controller.controlAvailable
            accessibleDescription: modelData === root.controller.currentScene
                                   ? qsTr("The live scene")
                                   : qsTr("Make this scene live")
            onClicked: root.controller.selectScene(modelData)
        }
    }

    Button {
        objectName: "obsAppletOpenObs"
        Layout.fillWidth: true
        text: qsTr("Open OBS")
        emphasized: false
        accessibleDescription: qsTr("Open the OBS window for everything this popup does not do")
        onClicked: root.controller.openObs()
    }
}
