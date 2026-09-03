// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var audioSettings
    signal closeRequested()

    readonly property Item firstFocusTarget: retryButton.visible
        ? retryButton
        : outputSection.firstActionTarget !== null
          ? outputSection.firstActionTarget
          : closeButton

    title: qsTr("Audio")

    Keys.onPressed: event => {
        const pageStep = Math.max(1, viewport.height - Tokens.space["5"])
        if (event.key === Qt.Key_PageDown) {
            viewport.contentY = Math.min(
                        Math.max(0, viewport.contentHeight - viewport.height),
                        viewport.contentY + pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            viewport.contentY = Math.max(0, viewport.contentY - pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_Home
                   && (event.modifiers & Qt.ControlModifier)) {
            viewport.contentY = 0
            event.accepted = true
        } else if (event.key === Qt.Key_End
                   && (event.modifiers & Qt.ControlModifier)) {
            viewport.contentY = Math.max(
                        0, viewport.contentHeight - viewport.height)
            event.accepted = true
        }
    }

    background: Rectangle { color: Tokens.bg.base }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "audioPageHeading"
            Layout.fillWidth: true
            text: qsTr("Audio")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        StateCard {
            id: serviceState
            objectName: "audioServiceState"
            Layout.fillWidth: true
            status: root.audioSettings.loading ? StateCard.Busy
                    : root.audioSettings.ready ? StateCard.Success
                    : root.audioSettings.stale || root.audioSettings.degraded
                      ? StateCard.Warning
                    : StateCard.Error
            title: root.audioSettings.stale
                   ? qsTr("Stale audio information")
                   : root.audioSettings.degraded
                     ? qsTr("Limited audio information")
                   : root.audioSettings.ready ? qsTr("Audio service ready")
                   : qsTr("Audio service unavailable")
            message: root.audioSettings.statusText
            actionText: root.audioSettings.reloadAvailable
                        && !root.audioSettings.ready ? qsTr("Retry") : ""
            onActionTriggered: root.audioSettings.reload()
        }

        Label {
            objectName: "audioOperationStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.audioSettings.operationStatusText
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "audioError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.audioSettings.errorText
            color: Tokens.status.warning.foreground
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "audioFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Audio settings scroll position")
            }

            function revealItem(focusedItem) {
                if (focusedItem === null || focusedItem === undefined
                        || focusedItem === viewport) {
                    return
                }
                let cursor = focusedItem
                let belongsToForm = false
                while (cursor !== null && cursor !== undefined) {
                    if (cursor === formSurface) {
                        belongsToForm = true
                        break
                    }
                    cursor = cursor.parent
                }
                if (!belongsToForm) {
                    return
                }
                const position = focusedItem.mapToItem(formSurface, 0, 0)
                const margin = Tokens.space["2"]
                if (position.y - margin < contentY) {
                    contentY = Math.max(0, position.y - margin)
                } else if (position.y + focusedItem.height + margin
                           > contentY + height) {
                    contentY = Math.min(
                                Math.max(0, contentHeight - height),
                                position.y + focusedItem.height + margin
                                - height)
                }
            }

            function revealActiveFocus() {
                if (root.Window.window !== null) {
                    revealItem(root.Window.window.activeFocusItem)
                }
            }

            onHeightChanged: Qt.callLater(revealActiveFocus)

            FormSurface {
                id: formSurface
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["4"]

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Default output: %1 · Default input: %2")
                            .arg(root.audioSettings.defaultOutputName.length > 0
                                 ? root.audioSettings.defaultOutputName
                                 : qsTr("none"))
                            .arg(root.audioSettings.defaultInputName.length > 0
                                 ? root.audioSettings.defaultInputName
                                 : qsTr("none"))
                        muted: true
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }

                    AudioDeviceSection {
                        id: outputSection
                        audioSettings: root.audioSettings
                        kindPrefix: "audioOutput"
                        sectionTitle: qsTr("Output devices")
                        sectionDescription: qsTr(
                            "Speakers and headphones with default-device selection")
                        deviceRows: root.audioSettings.outputDevices
                        emptyText: qsTr(
                            "No output devices are currently reported.")
                    }

                    AudioDeviceSection {
                        audioSettings: root.audioSettings
                        kindPrefix: "audioInput"
                        sectionTitle: qsTr("Input devices")
                        sectionDescription: qsTr(
                            "Microphones and capture devices with default-device selection")
                        deviceRows: root.audioSettings.inputDevices
                        emptyText: qsTr(
                            "No input devices are currently reported.")
                    }

                    AudioStreamSection {
                        audioSettings: root.audioSettings
                        streamRows: root.audioSettings.streams
                    }
                }
            }
        }

        Connections {
            target: root.Window.window
            enabled: root.Window.window !== null
            function onActiveFocusItemChanged() { viewport.revealActiveFocus() }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Button {
                id: retryButton
                objectName: "audioRetryButton"
                visible: root.audioSettings.reloadAvailable
                         && !root.audioSettings.ready
                available: root.audioSettings.reloadAvailable
                busy: root.audioSettings.loading
                emphasized: false
                text: qsTr("Retry")
                accessibleDescription: qsTr(
                    "Reconnect to the audio service and reload its state")
                onClicked: root.audioSettings.reload()
            }

            Label {
                Layout.fillWidth: true
                text: root.audioSettings.busy
                      ? qsTr("An audio change is in progress…")
                      : root.audioSettings.serviceEpoch > 0
                        ? qsTr("Epoch %1, revision %2")
                          .arg(root.audioSettings.serviceEpoch)
                          .arg(root.audioSettings.serviceRevision)
                        : ""
                muted: true
                Accessible.name: text
            }

            Button {
                id: closeButton
                objectName: "audioCloseButton"
                available: !root.audioSettings.busy
                emphasized: false
                text: qsTr("Close")
                KeyNavigation.tab: root.firstFocusTarget
                KeyNavigation.backtab: retryButton.visible ? retryButton
                                                            : closeButton
                onClicked: root.closeRequested()
            }
        }
    }
}
