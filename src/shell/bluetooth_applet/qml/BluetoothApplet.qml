// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null

    objectName: "bluetoothApplet"
    implicitWidth: 32
    implicitHeight: 28

    function closeDetailsFromEscape() {
        if (root.access !== null && root.access.pairingPromptVisible
                && !root.access.pairingReplyPending)
            root.access.cancelPrompt()
        details.close()
    }

    ToolButton {
        id: summary
        objectName: "bluetoothAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.access !== null
                         ? root.access.accessibleName
                         : qsTr("Bluetooth is unavailable")
        Accessible.description: root.access !== null
                                ? root.access.accessibleDescription : ""

        function openDetails() {
            if (root.available)
                details.open()
        }

        onClicked: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: ShellIcons.Icon {
            objectName: "bluetoothAppletIcon"
            anchors.centerIn: parent
            name: root.available && root.access.phase === "ready"
                  ? "network-bluetooth-activated"
                  : "network-bluetooth-inactive-symbolic"
            size: Math.min(20, root.height - 8)
            color: Tokens.fg.default
            symbolic: true
            fallbackText: qsTr("Bluetooth")
            Accessible.ignored: true
        }
        background: Item {}
    }

    Popup {
        id: details
        // AGENT-GUARD: panels reject keyboard focus and cannot paint outside
        // their surface. A separate popup window supplies both capabilities.
        popupType: Popup.Window
        objectName: "bluetoothAppletPopup"
        width: 340
        padding: 12
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            if (root.access !== null)
                root.access.setExpanded(true)
        }
        onClosed: {
            if (root.access !== null)
                root.access.setExpanded(false)
        }

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: root.colors.surfaceRaised ?? "#2c312e"
            border.color: root.colors.border ?? "#3c433f"
        }

        contentItem: ScrollView {
            implicitHeight: Math.min(contentColumn.implicitHeight, 460)
            clip: true

            // AGENT-GUARD: shortcut ownership follows the focusable popup
            // content window, never the non-focusable panel.
            Shortcut {
                // AGENT-GUARD: sequence needs a QKeySequence string; Qt.Key_Escape is
                // an integer key code and silently fails in the offscreen Quick path.
                sequence: "Escape"
                context: Qt.WindowShortcut
                enabled: details.opened
                onActivated: root.closeDetailsFromEscape()
            }

            ColumnLayout {
                id: contentColumn
                width: details.availableWidth
                spacing: 8
                Label {
                    objectName: "bluetoothAppletHeading"
                    Layout.fillWidth: true
                    text: qsTr("Bluetooth")
                    color: root.colors.text ?? "white"
                    font.bold: true
                    Accessible.role: Accessible.Heading
                }
                Label {
                    objectName: "bluetoothAppletLoading"
                    Layout.fillWidth: true
                    visible: root.access !== null
                             && root.access.phase === "loading"
                    text: qsTr("Bluetooth information is loading…")
                    color: root.colors.textMuted ?? "#a9afa9"
                    wrapMode: Text.Wrap
                }
                Label {
                    objectName: "bluetoothAppletDiagnostic"
                    Layout.fillWidth: true
                    visible: root.access !== null
                             && root.access.diagnostic !== ""
                    text: visible ? root.access.diagnostic : ""
                    color: root.colors.textMuted ?? "#a9afa9"
                    wrapMode: Text.Wrap
                }
                BluetoothPairingPrompt {
                    objectName: "bluetoothAppletPairingPrompt"
                    access: root.access
                    colors: root.colors
                    theme: root.theme
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.access !== null
                             && root.access.adapterRows.length > 0
                    text: qsTr("Adapters")
                    color: root.colors.text ?? "white"
                    font.bold: true
                }
                Repeater {
                    model: root.access !== null ? root.access.adapterRows : []

                    BluetoothAdapterRow {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                        access: root.access
                        colors: root.colors
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.access !== null
                             && root.access.deviceRows.length > 0
                    text: qsTr("Devices")
                    color: root.colors.text ?? "white"
                    font.bold: true
                }

                Repeater {
                    model: root.access !== null ? root.access.deviceRows : []

                    BluetoothDeviceRow {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                        access: root.access
                        colors: root.colors
                    }
                }

                Label {
                    objectName: "bluetoothAppletEmpty"
                    Layout.fillWidth: true
                    visible: root.access !== null
                             && root.access.phase === "ready"
                             && root.access.adapterRows.length === 0
                    text: qsTr("No Bluetooth adapters are available.")
                    color: root.colors.textMuted ?? "#a9afa9"
                    wrapMode: Text.Wrap
                }

                Label {
                    objectName: "bluetoothAppletFeedback"
                    Layout.fillWidth: true
                    visible: root.access !== null && root.access.feedbackPresent
                    text: visible ? root.access.feedback : ""
                    color: root.colors.warning ?? "#e5a84b"
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }
        }
    }
}
