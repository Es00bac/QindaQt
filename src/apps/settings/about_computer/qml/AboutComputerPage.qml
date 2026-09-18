// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var aboutComputerSettings
    signal closeRequested()

    readonly property Item firstFocusTarget: refreshButton
    readonly property bool compact: width < 560

    title: qsTr("About this computer")
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
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Label {
                objectName: "aboutComputerPageHeading"
                Layout.fillWidth: true
                text: qsTr("About this computer")
                font.pointSize: Tokens.type.title
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            Button {
                id: refreshButton
                objectName: "aboutComputerRefreshButton"
                text: qsTr("Refresh")
                onClicked: root.aboutComputerSettings.refresh()
                accessibleDescription: qsTr("Re-read every value on this page")
            }
            Button {
                id: copyReportButton
                objectName: "aboutComputerCopyReportButton"
                text: qsTr("Copy report")
                onClicked: root.aboutComputerSettings.copyReport()
                accessibleDescription: qsTr("Copy a plain-text report of this page to the clipboard")
            }
        }

        Label {
            objectName: "aboutComputerCopyStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.aboutComputerSettings.copyStatusText
            muted: true
            Accessible.role: Accessible.StatusBar
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "aboutComputerFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("About this computer scroll position")
            }

            FormSurface {
                id: formSurface
                width: parent.width
                padding: Tokens.space["3"]

                contentItem: ColumnLayout {
                    spacing: Tokens.space["4"]

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Tokens.space["1"]

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("System")
                        }
                        Label {
                            objectName: "aboutComputerHostname"
                            Layout.fillWidth: true
                            text: root.aboutComputerSettings.hostnamedAvailable
                                ? qsTr("Hostname: %1").arg(root.aboutComputerSettings.hostname)
                                : qsTr("Hostname unavailable")
                            Accessible.name: text
                        }
                        Label {
                            objectName: "aboutComputerHardware"
                            Layout.fillWidth: true
                            visible: root.aboutComputerSettings.hostnamedAvailable
                            text: qsTr("Hardware: %1 %2 (%3)")
                                .arg(root.aboutComputerSettings.hardwareVendor)
                                .arg(root.aboutComputerSettings.hardwareModel)
                                .arg(root.aboutComputerSettings.chassis)
                            Accessible.name: text
                        }
                        Label {
                            objectName: "aboutComputerKernel"
                            Layout.fillWidth: true
                            visible: root.aboutComputerSettings.hostnamedAvailable
                            text: qsTr("Kernel: %1 %2")
                                .arg(root.aboutComputerSettings.kernelName)
                                .arg(root.aboutComputerSettings.kernelRelease)
                            Accessible.name: text
                        }
                        Label {
                            objectName: "aboutComputerOperatingSystem"
                            Layout.fillWidth: true
                            visible: root.aboutComputerSettings.hostnamedAvailable
                            text: qsTr("Operating system: %1")
                                .arg(root.aboutComputerSettings.operatingSystemPrettyName)
                            Accessible.name: text
                        }
                        Label {
                            objectName: "aboutComputerVersion"
                            Layout.fillWidth: true
                            text: qsTr("QindaQt version: %1")
                                .arg(root.aboutComputerSettings.qindaqtVersion)
                            Accessible.name: text
                        }
                        Label {
                            objectName: "aboutComputerCheckpoint"
                            Layout.fillWidth: true
                            text: root.aboutComputerSettings.installedCheckpointAvailable
                                ? qsTr("Installed checkpoint: %1")
                                      .arg(root.aboutComputerSettings.installedCheckpoint)
                                : qsTr("Installed checkpoint unavailable")
                            muted: !root.aboutComputerSettings.installedCheckpointAvailable
                            Accessible.name: text
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Tokens.space["1"]

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Storage")
                        }
                        Label {
                            objectName: "aboutComputerDisk"
                            Layout.fillWidth: true
                            text: root.aboutComputerSettings.diskSummary
                            muted: !root.aboutComputerSettings.diskAvailable
                            Accessible.name: text
                        }
                        Label {
                            objectName: "aboutComputerMemory"
                            Layout.fillWidth: true
                            text: root.aboutComputerSettings.memorySummary
                            muted: !root.aboutComputerSettings.memoryAvailable
                            Accessible.name: text
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Tokens.space["1"]

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Battery")
                        }
                        Label {
                            objectName: "aboutComputerBattery"
                            Layout.fillWidth: true
                            text: root.aboutComputerSettings.batterySummary
                            muted: !root.aboutComputerSettings.batteryPresent
                            Accessible.name: text
                        }
                        Label {
                            objectName: "aboutComputerBatteryHealth"
                            Layout.fillWidth: true
                            visible: root.aboutComputerSettings.batteryPresent
                            text: root.aboutComputerSettings.batteryHealthSummary
                            Accessible.name: text
                        }
                    }
                }
            }
        }
    }
}
