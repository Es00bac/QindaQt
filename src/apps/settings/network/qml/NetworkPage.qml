// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var networkSettings
    signal closeRequested()

    readonly property Item firstFocusTarget: root.networkSettings.unavailable
                                                  ? reloadButton
                                                  : scanButton.enabled ? scanButton
                                                  : reloadButton.enabled ? reloadButton
                                                  : root

    title: qsTr("Network")

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
            objectName: "networkPageHeading"
            Layout.fillWidth: true
            text: qsTr("Network")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        StateCard {
            id: serviceState
            objectName: "networkServiceState"
            Layout.fillWidth: true
            status: root.networkSettings.loading ? StateCard.Busy
                    : root.networkSettings.ready ? StateCard.Success
                    : root.networkSettings.stale ? StateCard.Warning
                    : StateCard.Error
            title: root.networkSettings.unavailable
                   ? qsTr("Network unavailable")
                   : root.networkSettings.connectivityText
            message: root.networkSettings.statusText
        }

        // Reload belongs with the connection state. In particular, its
        // unavailable form is the immediate recovery action, not a detached
        // footer button below a hidden inventory.
        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Button {
                id: reloadButton
                objectName: "networkReloadButton"
                available: root.networkSettings.reloadAvailable
                busy: root.networkSettings.loading
                emphasized: root.networkSettings.unavailable
                text: root.networkSettings.unavailable ? qsTr("Try again")
                                                         : qsTr("Reload")
                accessibleDescription: root.networkSettings.unavailable
                                       ? qsTr("Reconnect the network service")
                                       : qsTr("Refresh network connections")
                onClicked: root.networkSettings.reload()
            }
        }

        Label {
            objectName: "networkOperationStatus"
            Layout.fillWidth: true
            visible: text.length > 0 && !root.networkSettings.unavailable
            text: root.networkSettings.operationStatusText
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "networkError"
            Layout.fillWidth: true
            visible: text.length > 0 && !root.networkSettings.unavailable
            text: root.networkSettings.errorText
            color: Tokens.fg.default
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        StateCard {
            objectName: "networkCredentialBoundary"
            visible: !root.networkSettings.secretAgentRegistered
                     && !root.networkSettings.unavailable
            Layout.fillWidth: true
            status: root.networkSettings.secretAgentRegistered
                    ? StateCard.Success : StateCard.Warning
            title: root.networkSettings.secretAgentRegistered
                   ? qsTr("Wi-Fi password prompts available")
                   : qsTr("Wi-Fi password prompts unavailable")
            message: root.networkSettings.secretAgentStatusText
        }

        Flickable {
            id: viewport
            objectName: "networkFormViewport"
            Layout.fillWidth: true
            // A hidden fill-height Flickable can still consume the spare
            // ColumnLayout height. Collapse it while unavailable so the
            // recovery notice and Try again action stay directly below the
            // page heading instead of separating across the window.
            Layout.fillHeight: !root.networkSettings.unavailable
            Layout.minimumHeight: 0
            Layout.preferredHeight: root.networkSettings.unavailable ? 0 : implicitHeight
            Layout.maximumHeight: root.networkSettings.unavailable
                                  ? 0 : Number.POSITIVE_INFINITY
            visible: !root.networkSettings.unavailable
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Network settings scroll position")
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
                } else if (position.y + focusedItem.height + margin > contentY + height) {
                    contentY = Math.min(Math.max(0, contentHeight - height),
                                        position.y + focusedItem.height + margin - height)
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

                    NetworkRadioSection {
                        networkSettings: root.networkSettings
                    }

                    NetworkDeviceSection {
                        networkSettings: root.networkSettings
                    }

                    NetworkSavedSection {
                        networkSettings: root.networkSettings
                    }

                    NetworkAccessPointSection {
                        networkSettings: root.networkSettings
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
                id: scanButton
                objectName: "networkScanButton"
                visible: !root.networkSettings.unavailable
                available: root.networkSettings.scanAvailable
                busy: root.networkSettings.busy
                text: qsTr("Scan")
                accessibleDescription: root.networkSettings.scanStatusText
                onClicked: root.networkSettings.requestScan()
            }

            Label {
                Layout.fillWidth: true
                visible: !root.networkSettings.unavailable
                text: root.networkSettings.scanStatusText
                muted: true
                Accessible.name: text
            }

        }
    }
}
