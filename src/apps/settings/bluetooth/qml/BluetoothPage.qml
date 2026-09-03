// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var bluetoothSettings
    signal closeRequested()

    // AGENT-GUARD: Settings navigation enters the route through this target.
    // Close is always admitted and enabled, including empty, unavailable, and
    // busy states; the host must never focus a disabled domain action.
    readonly property Item firstFocusTarget: closeButton

    title: qsTr("Bluetooth")
    background: Rectangle { color: Tokens.bg.base }

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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "bluetoothPageHeading"
            Layout.fillWidth: true
            text: qsTr("Bluetooth")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        StateCard {
            objectName: "bluetoothServiceState"
            Layout.fillWidth: true
            status: root.bluetoothSettings.loading ? StateCard.Busy
                    : root.bluetoothSettings.ready ? StateCard.Success
                    : root.bluetoothSettings.degraded ? StateCard.Warning
                    : StateCard.Error
            title: root.bluetoothSettings.ready
                   ? qsTr("Bluetooth ready") : qsTr("Bluetooth unavailable")
            message: root.bluetoothSettings.statusText
        }

        Label {
            objectName: "bluetoothOperationStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.bluetoothSettings.operationStatusText
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "bluetoothError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.bluetoothSettings.errorText
            color: Tokens.status.warning.foreground
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "bluetoothFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Bluetooth settings scroll position")
            }

            function revealItem(focusedItem) {
                if (focusedItem === null || focusedItem === undefined) return
                const position = focusedItem.mapToItem(formSurface, 0, 0)
                const margin = Tokens.space["2"]
                if (position.y - margin < contentY) {
                    contentY = Math.max(0, position.y - margin)
                } else if (position.y + focusedItem.height + margin
                           > contentY + height) {
                    contentY = Math.min(Math.max(0, contentHeight - height),
                                        position.y + focusedItem.height
                                        + margin - height)
                }
            }

            function revealActiveFocus() {
                if (root.Window.window !== null
                        && root.Window.window.activeFocusItem !== closeButton) {
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

                    BluetoothPairingSection {
                        bluetoothSettings: root.bluetoothSettings
                    }

                    BluetoothAdapterSection {
                        bluetoothSettings: root.bluetoothSettings
                    }

                    BluetoothDeviceSection {
                        bluetoothSettings: root.bluetoothSettings
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

            Label {
                Layout.fillWidth: true
                text: root.bluetoothSettings.busy
                      ? qsTr("A Bluetooth operation is pending.") : ""
                muted: true
                Accessible.name: text
            }

            Button {
                id: closeButton
                objectName: "bluetoothCloseButton"
                available: true
                busy: false
                emphasized: false
                text: qsTr("Close")
                accessibleDescription: qsTr("Close Settings and release any discovery lease")
                onClicked: root.closeRequested()
            }
        }
    }
}
