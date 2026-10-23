// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
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
    Component.onDestruction: {
        if (root.access !== null)
            root.access.setExpanded(false);
    }

    function closeDetailsFromEscape() {
        if (root.access !== null && root.access.pairingPromptVisible && !root.access.pairingReplyPending)
            root.access.cancelPrompt();
        details.close();
    }

    ToolButton {
        id: summary
        objectName: "bluetoothAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.access !== null ? root.access.accessibleName : qsTr("Bluetooth is unavailable")
        Accessible.description: root.access !== null ? root.access.accessibleDescription : ""

        function openDetails() {
            if (root.available) {
                if (details.opened)
                    details.close();
                else
                    details.open();
            }
        }

        onClicked: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: ShellIcons.Icon {
            objectName: "bluetoothAppletIcon"
            anchors.centerIn: parent
            name: root.available && root.access.phase === "ready" ? "network-bluetooth-activated" : "network-bluetooth-inactive-symbolic"
            size: Math.min(20, root.height - 8)
            color: Tokens.fg.default
            symbolic: true
            fallbackText: qsTr("Bluetooth")
            Accessible.ignored: true
        }
        background: Item {}
    }

    C.PanelPopup {
        id: details

        // AGENT-CONTRACT: placement is owned by QindaQt.Controls.PanelPopup, the
        // one owner of panel-popup placement (docs/wiki/shell/panel-popup-placement.md):
        // it opens flush with the control's leading edge, above it on a bottom
        // panel, beside it on a side panel, and slides to stay on the output.
        // It also keeps the surface a real window, because a layer-shell panel
        // rejects keyboard focus and cannot paint outside its own band.
        anchorItem: summary
        vertical: root.vertical
        objectName: "bluetoothAppletPopup"
        width: Math.min(340, Math.max(1, outputWidth() - 24))
        padding: 12

        onOpened: {
            if (root.access !== null)
                root.access.setExpanded(true);
        }
        onClosed: {
            if (root.access !== null)
                root.access.setExpanded(false);
        }

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: root.colors.surfaceRaised ?? "#2c312e"
            border.color: root.colors.border ?? "#3c433f"
        }

        contentItem: ColumnLayout {
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Label {
                    objectName: "bluetoothAppletHeading"
                    Layout.fillWidth: true
                    text: qsTr("Bluetooth")
                    font.bold: true
                    color: Tokens.fg.default
                    Accessible.role: Accessible.Heading
                }
                C.Button {
                    objectName: "bluetoothAppletSettingsButton"
                    text: qsTr("Settings…")
                    emphasized: false
                    visible: Boolean(root.access?.canOpenSettings)
                    accessibleDescription: qsTr("Open the Bluetooth page of Settings")
                    onClicked: {
                        if (root.access.openSettings())
                            details.close();
                    }
                }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                implicitHeight: Math.min(contentColumn.implicitHeight, 360, Math.max(1, details.outputHeight() - 112))
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
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
                    spacing: 6

                    Label {
                        objectName: "bluetoothAppletLoading"
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.phase === "loading"
                        text: qsTr("Bluetooth information is loading…")
                        color: root.colors.textMuted ?? "#a9afa9"
                        wrapMode: Text.Wrap
                    }
                    Label {
                        objectName: "bluetoothAppletDiagnostic"
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.diagnostic !== ""
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
                        visible: root.access !== null && root.access.adapterRows.length > 0
                        text: qsTr("Adapters")
                        color: root.colors.text ?? "white"
                        font.bold: true
                    }
                    Repeater {
                        model: root.available ? (root.access.adapterRows ?? []).length : 0

                        BluetoothAdapterRow {
                            required property int index
                            readonly property var modelData: root.access?.adapterRows?.[index] ?? ({
                                    "label": "",
                                    "id": "",
                                    "powered": false,
                                    "canSetPowered": false,
                                    "canReleaseDiscovery": false,
                                    "canAcquireDiscovery": false,
                                    "accessibleName": "",
                                    "accessibleDescription": ""
                                })
                            Layout.fillWidth: true
                            row: modelData
                            access: root.access
                            colors: root.colors
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.deviceRows.length > 0
                        text: qsTr("Devices")
                        color: root.colors.text ?? "white"
                        font.bold: true
                    }

                    Repeater {
                        model: root.available ? (root.access.deviceRows ?? []).length : 0

                        BluetoothDeviceRow {
                            required property int index
                            readonly property var modelData: root.access?.deviceRows?.[index] ?? ({
                                    "label": "",
                                    "id": "",
                                    "paired": false,
                                    "connected": false,
                                    "pending": false,
                                    "trusted": false,
                                    "canPair": false,
                                    "canSetTrusted": false,
                                    "canRemove": false,
                                    "canConnect": false,
                                    "canDisconnect": false,
                                    "accessibleName": "",
                                    "accessibleDescription": ""
                                })
                            Layout.fillWidth: true
                            row: modelData
                            access: root.access
                            colors: root.colors
                        }
                    }

                    Label {
                        objectName: "bluetoothAppletEmpty"
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.phase === "ready" && root.access.adapterRows.length === 0
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
}
