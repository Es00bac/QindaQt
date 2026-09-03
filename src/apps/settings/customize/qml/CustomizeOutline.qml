// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.ScrollView {
    id: root

    required property var customizeSettings
    readonly property Item firstFocusTarget: outlineColumn.children.length > 1
                                                  ? outlineColumn.children[1]
                                                  : null
    clip: true
    Accessible.role: Accessible.List
    Accessible.name: qsTr("Layout outline")

    ColumnLayout {
        id: outlineColumn
        width: root.availableWidth
        spacing: Tokens.space["1"]

        SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Layout outline")
            description: qsTr("Keyboard-accessible panels, zones, and applets")
        }

        Repeater {
            model: root.customizeSettings.panels

            delegate: ColumnLayout {
                id: panelGroup
                required property var modelData
                Layout.fillWidth: true
                spacing: Tokens.space["1"]

                Button {
                    objectName: "customizeOutlinePanel_" + panelGroup.modelData.id
                    Layout.fillWidth: true
                    text: panelGroup.modelData.name
                    emphasized: root.customizeSettings.selectedKind === "panel"
                                && root.customizeSettings.selectedPanelId
                                   === panelGroup.modelData.id
                    available: root.customizeSettings.canEdit
                    Accessible.role: Accessible.ListItem
                    Accessible.name: qsTr("%1, panel").arg(text)
                    Accessible.description: qsTr("%1 applets")
                                                .arg(panelGroup.modelData.applets.length)
                    onClicked: root.customizeSettings.selectPanel(
                                   panelGroup.modelData.id)
                }

                Repeater {
                    model: ["start", "center", "end"]

                    delegate: ColumnLayout {
                        id: zoneGroup
                        required property string modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: Tokens.space["3"]
                        spacing: Tokens.space["1"]

                        readonly property var zoneApplets:
                            panelGroup.modelData.applets.filter(
                                item => item.zone === zoneGroup.modelData)
                        Button {
                            objectName: "customizeOutlineZone_"
                                        + panelGroup.modelData.id + "_"
                                        + zoneGroup.modelData
                            Layout.fillWidth: true
                            text: qsTr("%1 zone").arg(zoneGroup.modelData)
                            emphasized: false
                            available: root.customizeSettings.canEdit
                            Accessible.role: Accessible.ListItem
                            Accessible.name: text
                            Accessible.description: qsTr("%1 applets")
                                                        .arg(zoneGroup.zoneApplets.length)
                        }
                        Repeater {
                            model: zoneGroup.zoneApplets

                            delegate: Button {
                                id: appletButton
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.leftMargin: Tokens.space["3"]
                                text: appletButton.modelData.name
                                emphasized: root.customizeSettings.selectedKind
                                            === "applet"
                                            && root.customizeSettings.selectedAppletId
                                               === appletButton.modelData.id
                                available: root.customizeSettings.canEdit
                                Accessible.role: Accessible.ListItem
                                Accessible.name: qsTr("%1 applet, position %2 of %3")
                                    .arg(text)
                                    .arg(appletButton.modelData.position)
                                    .arg(appletButton.modelData.count)
                                Accessible.description: qsTr("Press Space to move; Delete to remove")
                                onClicked: root.customizeSettings.selectApplet(
                                    panelGroup.modelData.id,
                                    appletButton.modelData.id)
                                Keys.onSpacePressed: event => {
                                    root.customizeSettings.selectApplet(
                                        panelGroup.modelData.id,
                                        appletButton.modelData.id)
                                    root.customizeSettings.keyboardMoveMode()
                                    event.accepted = true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
