// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

// Bounded audio panel applet surface. The controller is the composed shell
// facade injected above QML; this file never imports the Audio1 client or
// any service module and owns no business policy of its own.
Item {
    id: root

    objectName: "audioApplet"
    implicitWidth: 340
    implicitHeight: content.implicitHeight

    property var controller: null

    readonly property bool showLists:
        controller?.phaseText === "ready"
            || controller?.phaseText === "degraded"

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Audio")
    Accessible.description: {
        if (!controller)
            return qsTr("Audio controls are not connected")
        if (controller.phaseText === "loading")
            return qsTr("Audio device information is loading")
        if (controller.phaseText === "unavailable")
            return qsTr("Audio is unavailable")
        return qsTr("Volume and mute controls for audio devices and application streams")
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 12

        C.SectionHeader {
            objectName: "audioSectionHeader"
            Layout.fillWidth: true
            title: qsTr("Audio")
            description: showLists && controller.hasDefaultOutput
                ? qsTr("Default output: %1").arg(controller.defaultOutputLabel)
                : ""
        }

        C.Label {
            objectName: "audioDefaultInputLabel"
            Layout.fillWidth: true
            visible: showLists
            text: controller && controller.hasDefaultInput
                ? qsTr("Default input: %1").arg(controller.defaultInputLabel)
                : qsTr("No default input")
            muted: !(controller && controller.hasDefaultInput)
        }

        C.StateCard {
            objectName: "audioLoadingState"
            Layout.fillWidth: true
            visible: controller?.phaseText === "loading"
            status: C.StateCard.Busy
            title: qsTr("Audio")
            message: qsTr("Audio device information is loading…")
        }

        C.DegradedNotice {
            objectName: "audioUnavailableNotice"
            Layout.fillWidth: true
            visible: controller?.phaseText === "unavailable"
            title: qsTr("Audio is unavailable")
            reason: controller?.phaseReasonText ?? ""
        }

        C.DegradedNotice {
            objectName: "audioDegradedNotice"
            Layout.fillWidth: true
            visible: controller?.phaseText === "degraded"
            title: qsTr("Audio information is limited")
            reason: controller?.phaseReasonText ?? ""
        }

        C.StateCard {
            objectName: "audioFeedbackState"
            Layout.fillWidth: true
            visible: controller?.feedbackPresent ?? false
            status: C.StateCard.Error
            title: qsTr("Change not applied")
            message: controller?.feedback ?? ""
            actionText: qsTr("Dismiss")
            onActionTriggered: controller.clearFeedback()
        }

        C.Label {
            objectName: "audioEmptyDevices"
            Layout.fillWidth: true
            visible: showLists && controller.deviceRows.length === 0
            text: qsTr("No audio devices are reported right now.")
            muted: true
        }

        Repeater {
            objectName: "audioDeviceRows"
            model: showLists ? controller.deviceRows : []

            delegate: AudioDeviceRow {
                required property var modelData

                Layout.fillWidth: true
                row: modelData
                controller: root.controller
            }
        }

        C.Label {
            objectName: "audioDeviceOverflow"
            Layout.fillWidth: true
            visible: showLists && controller.overflowDeviceCount > 0
            text: controller ? qsTr("%1 more devices are managed in Audio settings.").arg(
                                   controller.overflowDeviceCount) : ""
            muted: true
        }

        C.SectionHeader {
            objectName: "audioStreamsHeader"
            Layout.fillWidth: true
            visible: showLists && controller.streamRows.length > 0
            title: qsTr("Application streams")
        }

        Repeater {
            objectName: "audioStreamRows"
            model: showLists ? controller.streamRows : []

            delegate: AudioStreamRow {
                required property var modelData

                Layout.fillWidth: true
                row: modelData
                controller: root.controller
            }
        }

        C.Label {
            objectName: "audioStreamOverflow"
            Layout.fillWidth: true
            visible: showLists && controller.overflowStreamCount > 0
            text: controller ? qsTr("%1 more streams are managed in Audio settings.").arg(
                                   controller.overflowStreamCount) : ""
            muted: true
        }
    }
}
