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
    readonly property bool anyOn: available && access.onCount > 0

    objectName: "smartLightsApplet"
    implicitWidth: vertical ? 32 : (root.available && root.access.deviceCount > 0 ? 50 : 32)
    implicitHeight: 28

    ToolButton {
        id: summary
        objectName: "smartLightsAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.available ? root.access.accessibleName
                                        : qsTr("Smart lights are unavailable")
        Accessible.description: root.available ? root.access.accessibleDescription : ""

        function openDetails() {
            if (root.available)
                details.open()
        }

        onClicked: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: RowLayout {
            spacing: 3

            ShellIcons.Icon {
                objectName: "smartLightsAppletIcon"
                name: root.anyOn ? "brightness-high" : "brightness-low"
                size: Math.min(20, root.height - 8)
                // A lit room is worth seeing at a glance, so the chip takes the
                // accent colour exactly when a light is on.
                color: root.anyOn ? Tokens.accent.default : Tokens.fg.default
                symbolic: true
                fallbackText: qsTr("Lights")
                Accessible.ignored: true
            }

            Text {
                objectName: "smartLightsAppletCount"
                visible: !root.vertical && root.available && root.access.deviceCount > 0
                text: root.available ? String(root.access.onCount) : ""
                color: Tokens.fg.default
                font.pixelSize: 11
                textFormat: Text.PlainText
                Accessible.ignored: true
            }
        }
        background: Item {}
    }

    Popup {
        id: details
        // AGENT-GUARD: panels reject keyboard focus and cannot paint outside
        // their surface. A separate popup window supplies both capabilities.
        popupType: Popup.Window
        objectName: "smartLightsAppletPopup"
        width: 380
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
            implicitHeight: Math.min(contentColumn.implicitHeight, 520)
            clip: true

            // AGENT-GUARD: shortcut ownership follows the focusable popup
            // content window, never the non-focusable panel.
            Shortcut {
                sequence: "Escape"
                context: Qt.WindowShortcut
                enabled: details.opened
                onActivated: details.close()
            }

            ColumnLayout {
                id: contentColumn
                width: details.availableWidth
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        objectName: "smartLightsAppletHeading"
                        Layout.fillWidth: true
                        text: qsTr("Smart lights")
                        color: root.colors.text ?? "white"
                        font.bold: true
                        Accessible.role: Accessible.Heading
                    }

                    C.Button {
                        objectName: "smartLightsAppletSearchButton"
                        emphasized: false
                        text: root.available && root.access.discovering
                              ? qsTr("Searching…") : qsTr("Search")
                        available: root.available && root.access.controlAvailable
                        accessibleDescription: qsTr("Look for smart lights on this network")
                        onClicked: root.access.requestDiscovery()
                    }
                }

                Label {
                    objectName: "smartLightsAppletLoading"
                    Layout.fillWidth: true
                    visible: root.available && root.access.phase === "loading"
                    text: qsTr("Looking for lights on this network…")
                    color: root.colors.textMuted ?? "#a9afa9"
                    wrapMode: Text.Wrap
                }

                Label {
                    objectName: "smartLightsAppletDiagnostic"
                    Layout.fillWidth: true
                    visible: root.available && root.access.diagnostic !== ""
                    text: visible ? root.access.diagnostic : ""
                    color: root.colors.textMuted ?? "#a9afa9"
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.available && root.access.deviceCount > 0
                    spacing: 8

                    C.Button {
                        objectName: "smartLightsAppletAllOnButton"
                        Layout.fillWidth: true
                        emphasized: false
                        text: qsTr("All on")
                        available: root.available && root.access.controlAvailable
                        accessibleDescription: qsTr("Switch on every light that is responding")
                        onClicked: root.access.requestAllPower(true)
                    }

                    C.Button {
                        objectName: "smartLightsAppletAllOffButton"
                        Layout.fillWidth: true
                        emphasized: false
                        text: qsTr("All off")
                        available: root.available && root.access.controlAvailable
                        accessibleDescription: qsTr("Switch off every light that is responding")
                        onClicked: root.access.requestAllPower(false)
                    }
                }

                Repeater {
                    model: root.available ? root.access.deviceRows : []

                    SmartLightDeviceRow {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                        access: root.access
                        colors: root.colors
                    }
                }

                Label {
                    objectName: "smartLightsAppletEmpty"
                    Layout.fillWidth: true
                    visible: root.available && root.access.phase === "ready"
                             && root.access.deviceCount === 0
                    text: qsTr("No smart lights have answered yet. Make sure they are "
                               + "powered on and joined to this network, then search again.")
                    color: root.colors.textMuted ?? "#a9afa9"
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.available && root.access.presetRows.length > 0
                    text: qsTr("Saved arrangements")
                    color: root.colors.text ?? "white"
                    font.bold: true
                    Accessible.role: Accessible.Heading
                }

                Repeater {
                    model: root.available ? root.access.presetRows : []

                    SmartLightPresetRow {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                        access: root.access
                        colors: root.colors
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.available && root.access.deviceCount > 0
                    spacing: 8

                    C.TextField {
                        id: presetName
                        objectName: "smartLightsAppletPresetName"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Name this arrangement")
                        accessibleName: qsTr("Name for the current lighting arrangement")
                        onAccepted: saveButton.commit()
                    }

                    C.Button {
                        id: saveButton
                        objectName: "smartLightsAppletSavePresetButton"
                        emphasized: false
                        text: qsTr("Save")
                        available: root.available && root.access.controlAvailable
                                   && presetName.text.trim().length > 0
                        accessibleDescription: qsTr("Save what the lights are doing now")

                        function commit() {
                            if (!available)
                                return
                            if (root.access.savePreset(presetName.text))
                                presetName.text = ""
                        }

                        onClicked: commit()
                    }
                }

                Label {
                    objectName: "smartLightsAppletFeedback"
                    Layout.fillWidth: true
                    visible: root.available && root.access.feedbackPresent
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
