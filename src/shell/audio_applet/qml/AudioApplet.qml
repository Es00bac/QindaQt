// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Bounded audio panel applet surface. The controller is the composed shell
// facade injected above QML; this file never imports the Audio1 client or
// any service module and owns no business policy of its own.
Item {
    id: root

    objectName: "audioApplet"
    implicitWidth: 32
    implicitHeight: 28

    property var controller: null
    property bool vertical: false

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

    function defaultOutputRow() {
        if (!controller)
            return null
        const rows = controller.deviceRows ?? []
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].isOutput === true && rows[i].isDefault === true)
                return rows[i]
        }
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].isOutput === true)
                return rows[i]
        }
        return null
    }

    readonly property var outputRow: defaultOutputRow()
    readonly property string summaryIconName: {
        if (!outputRow || outputRow.muted)
            return "audio-volume-muted"
        const volume = Number(outputRow.volume ?? 0)
        if (volume <= 0) return "audio-volume-muted"
        if (volume < 1 / 3) return "audio-volume-low"
        if (volume < 2 / 3) return "audio-volume-medium"
        return "audio-volume-high"
    }

    T.ToolButton {
        id: summary
        objectName: "audioAppletSummary"
        anchors.fill: parent
        enabled: root.controller !== null
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.Accessible.name
        Accessible.description: root.Accessible.description

        function toggleDetails() {
            if (!enabled)
                return
            if (details.opened) details.close()
            else details.open()
        }
        onClicked: toggleDetails()
        Accessible.onPressAction: toggleDetails()
        Keys.onReturnPressed: event => {
            toggleDetails()
            event.accepted = true
        }
        Keys.onEnterPressed: event => {
            toggleDetails()
            event.accepted = true
        }

        contentItem: ShellIcons.Icon {
            objectName: "audioAppletIcon"
            anchors.centerIn: parent
            name: root.summaryIconName
            size: Math.min(20, root.height - Tokens.space["2"])
            color: Tokens.fg.default
            symbolic: true
            fallbackText: qsTr("Audio")
            Accessible.ignored: true
        }
        background: Item {}
    }

    T.Popup {
        id: details
        objectName: "audioAppletPopup"
        popupType: T.Popup.Window
        modal: false
        focus: true
        padding: Tokens.space["3"]
        width: 360
        height: Math.min(560, Math.max(160, content.implicitHeight + padding * 2))
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
                     | T.Popup.CloseOnPressOutsideParent

        background: Rectangle {
            radius: Tokens.radius.l
            color: Tokens.bg.raised
            border.color: Tokens.outline.divider
        }

        contentItem: T.ScrollView {
            clip: true

            ColumnLayout {
                id: content
                width: details.availableWidth
                spacing: Tokens.space["3"]

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
    }
}
