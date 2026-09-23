// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk
import QindaTK.QindaQt

T.Page {
    id: root

    // The console below is QindaTK (ADR-0227); the bridge feeds the desktop's
    // QST-1 tokens into the toolkit theme so the mixer wears the same theme
    // as the rest of the route.
    QindaQtTheme {}

    required property var audioSettings
    signal closeRequested()
    property int activeTab: 0

    function selectTab(index) {
        if (index < 0 || index > 2) return
        root.activeTab = index
        viewport.contentY = 0
    }

    // AGENT-GUARD: host entry/exit targets must belong to the VISIBLE tab.
    // A hidden device control must never absorb Settings keyboard entry when
    // the mixer or peer editor is selected. The mixer uses its TabStrip as
    // entry until its dense QindaTK controls expose one stable target.
    readonly property Item firstFocusTarget:
        activeTab === 0
            ? (outputSection.firstActionTarget !== null ? outputSection.firstActionTarget
               : inputSection.firstActionTarget !== null ? inputSection.firstActionTarget
               : virtualSection.firstActionTarget !== null ? virtualSection.firstActionTarget
               : streamSection.firstActionTarget !== null ? streamSection.firstActionTarget
               : retryButton.visible ? retryButton : destinationTabs)
        : activeTab === 2 && peerSection.firstActionTarget !== null
            ? peerSection.firstActionTarget : destinationTabs

    readonly property Item lastActionTarget:
        activeTab === 0
            ? (streamSection.lastActionTarget !== null ? streamSection.lastActionTarget
               : virtualSection.lastActionTarget !== null ? virtualSection.lastActionTarget
               : inputSection.lastActionTarget !== null ? inputSection.lastActionTarget
               : outputSection.lastActionTarget)
        : activeTab === 2 && peerSection.lastActionTarget !== null
            ? peerSection.lastActionTarget : destinationTabs

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
        anchors.margins: Tokens.space["4"]
        spacing: Tokens.space["2"]

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
            color: Tokens.fg.default
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Tk.TabStrip {
            id: destinationTabs
            objectName: "audioDestinationTabs"
            Layout.fillWidth: true
            model: [qsTr("Devices"), qsTr("Mixer"), qsTr("Other computers")]
            currentIndex: root.activeTab
            small: width < 480
            stretch: true
            Accessible.name: qsTr("Audio settings sections")
            onTabActivated: index => root.selectTab(index)
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
            onContentHeightChanged: Qt.callLater(revealActiveFocus)

            FormSurface {
                id: formSurface
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["3"]

                    Label {
                        visible: root.activeTab === 0
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

                    AudioConsoleSection {
                        id: consoleSection
                        audioSettings: root.audioSettings
                        visible: root.activeTab === 1
                                 && (root.audioSettings.consoleAvailable ?? false)
                    }

                    AudioPeerSection {
                        id: peerSection
                        audioSettings: root.audioSettings
                        visible: root.activeTab === 2
                    }

                    AudioDeviceSection {
                        id: outputSection
                        audioSettings: root.audioSettings
                        visible: root.activeTab === 0
                        kindPrefix: "audioOutput"
                        sectionTitle: qsTr("Output devices")
                        sectionDescription: qsTr(
                            "Speakers and headphones with default-device selection")
                        deviceRows: root.audioSettings.outputDevices
                        emptyText: qsTr(
                            "No output devices are currently reported.")
                    }

                    AudioDeviceSection {
                        id: inputSection
                        audioSettings: root.audioSettings
                        visible: root.activeTab === 0
                        kindPrefix: "audioInput"
                        sectionTitle: qsTr("Input devices")
                        sectionDescription: qsTr(
                            "Microphones and capture devices with default-device selection")
                        deviceRows: root.audioSettings.inputDevices
                        emptyText: qsTr(
                            "No input devices are currently reported.")
                    }

                    AudioVirtualDeviceSection {
                        id: virtualSection
                        audioSettings: root.audioSettings
                        visible: root.activeTab === 0
                        virtualDeviceRows: root.audioSettings.virtualDevices
                    }

                    AudioStreamSection {
                        id: streamSection
                        audioSettings: root.audioSettings
                        visible: root.activeTab === 0
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

                        : ""
                muted: true
                Accessible.name: text
            }

        }
    }
}
