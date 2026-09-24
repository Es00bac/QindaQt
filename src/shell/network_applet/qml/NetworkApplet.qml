// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Panel Network applet (docs/wiki/shell/network-applet.md, ADR-0258). All
// truth and every request go through the shell-private controller over the
// public Network1 client; this file only presents bounded rows.
Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null
    readonly property bool troubled: available
        && (access.requestPhase === "failed" || access.requestPhase === "uncertain")

    objectName: "networkApplet"
    implicitWidth: 32
    implicitHeight: 28

    ToolButton {
        id: summary
        objectName: "networkAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.available ? root.access.accessibleName
                                        : qsTr("Network is unavailable")
        Accessible.description: root.available ? root.access.accessibleDescription : ""

        function openDetails() {
            if (root.available)
                details.open()
        }

        onClicked: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: ShellIcons.Icon {
            objectName: "networkAppletIcon"
            anchors.centerIn: parent
            // AGENT-NOTE: the glyph vocabulary is indicatorIconName() in
            // network_applet_presentation.cpp; the icon coverage test pins it.
            name: root.available ? root.access.iconName : "network-offline"
            size: Math.min(20, root.height - 8)
            color: Tokens.fg.default
            symbolic: true
            fallbackText: qsTr("Network")
            Accessible.ignored: true
        }
        background: Item {}
    }

    C.PanelPopup {
        id: details

        // AGENT-CONTRACT: placement is owned by QindaQt.Controls.PanelPopup
        // (docs/wiki/shell/panel-popup-placement.md); it keeps the surface a
        // real, focusable window beside or above the panel.
        anchorItem: summary
        vertical: root.vertical
        objectName: "networkAppletPopup"
        width: 340
        padding: 12

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
            // window; the string form is required on the offscreen path.
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

                Label {
                    objectName: "networkAppletHeading"
                    Layout.fillWidth: true
                    text: qsTr("Network")
                    color: root.colors.text ?? "white"
                    font.bold: true
                    Accessible.role: Accessible.Heading
                }
                Label {
                    objectName: "networkAppletStatus"
                    Layout.fillWidth: true
                    visible: root.available && root.access.phase !== "loading"
                             && root.access.phase !== "unavailable"
                    text: visible ? root.access.summaryLabel : ""
                    color: root.colors.text ?? "white"
                    wrapMode: Text.Wrap
                }
                Label {
                    objectName: "networkAppletDiagnostic"
                    Layout.fillWidth: true
                    visible: root.available && root.access.diagnostic !== ""
                    text: visible ? root.access.diagnostic : ""
                    color: root.colors.textMuted ?? "#a9afa9"
                    wrapMode: Text.Wrap
                }

                Repeater {
                    model: root.available ? root.access.radioRows : []

                    C.Switch {
                        id: radioSwitch
                        required property var modelData
                        objectName: "networkAppletRadioSwitch"
                        Layout.fillWidth: true
                        text: modelData.pending ? qsTr("%1 (changing…)").arg(modelData.label)
                                                : modelData.label
                        enabled: modelData.canToggle && !modelData.pending
                        // AGENT-GUARD: the switch shows confirmed truth only.
                        // It never toggles itself, so a pending request can
                        // never read as done before Network1 confirms it.
                        checkable: false
                        checked: modelData.enabled
                        accessibleDescription: modelData.accessibleDescription
                        onClicked: root.access.requestRadio(radioSwitch.modelData.id,
                                                            !radioSwitch.modelData.enabled)
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.available && root.access.connectionRows.length > 0
                    text: qsTr("Connected")
                    color: root.colors.text ?? "white"
                    font.bold: true
                    Accessible.role: Accessible.Heading
                }
                Repeater {
                    model: root.available ? root.access.connectionRows : []

                    NetworkConnectionRow {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                        access: root.access
                        colors: root.colors
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.available && root.access.wifiDevicePresent
                             && (root.access.phase === "ready"
                                 || root.access.phase === "degraded")
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Wi-Fi networks")
                        color: root.colors.text ?? "white"
                        font.bold: true
                        Accessible.role: Accessible.Heading
                    }
                    C.Button {
                        objectName: "networkAppletRescanButton"
                        text: root.available && root.access.scanning ? qsTr("Searching…")
                                                                     : qsTr("Rescan")
                        emphasized: false
                        available: root.available && root.access.scanAvailable
                        accessibleDescription: qsTr("Search again for nearby Wi-Fi networks")
                        onClicked: root.access.requestScan()
                    }
                }
                Repeater {
                    model: root.available ? root.access.accessPointRows : []

                    NetworkAccessPointRow {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                        access: root.access
                        colors: root.colors
                    }
                }
                Label {
                    objectName: "networkAppletEmpty"
                    Layout.fillWidth: true
                    visible: root.available && root.access.phase === "ready"
                             && root.access.accessPointRows.length === 0
                    text: root.available && root.access.wifiDevicePresent
                          ? qsTr("No Wi-Fi networks found.")
                          : qsTr("No Wi-Fi adapter is available.")
                    color: root.colors.textMuted ?? "#a9afa9"
                    wrapMode: Text.Wrap
                }

                Label {
                    objectName: "networkAppletFeedback"
                    Layout.fillWidth: true
                    visible: root.available && root.access.feedbackPresent
                    text: visible ? root.access.feedback : ""
                    // Colour reinforces the words; the text itself says
                    // whether the change is pending, done, failed, or uncertain.
                    color: root.troubled ? (root.colors.warning ?? "#e5a84b")
                                         : (root.colors.textMuted ?? "#a9afa9")
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }

                C.Button {
                    objectName: "networkAppletSettingsButton"
                    Layout.fillWidth: true
                    visible: root.available && root.access.canOpenSettings
                    text: qsTr("Network Settings…")
                    emphasized: false
                    accessibleDescription: qsTr("Open the Network page of Settings")
                    onClicked: {
                        if (root.access.openSettings())
                            details.close()
                    }
                }
            }
        }
    }
}
