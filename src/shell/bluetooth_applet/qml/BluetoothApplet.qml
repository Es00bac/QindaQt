// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null

    objectName: "bluetoothApplet"
    implicitWidth: vertical ? 40 : Math.max(52, summary.implicitWidth + 12)
    implicitHeight: vertical ? 40 : 28

    ToolButton {
        id: summary
        objectName: "bluetoothAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: root.access !== null ? root.access.summaryLabel : qsTr("Bluetooth")
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

        contentItem: Text {
            text: root.vertical ? qsTr("BT") : summary.text
            color: root.colors.text ?? "white"
            font.pixelSize: root.vertical ? 10 : 11
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
        }
        background: Item {}
    }

    Popup {
        id: details
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
