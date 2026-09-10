// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Contextual inspector for the current selection. Panels get a compass,
// alignment glyphs, sliders, and a visibility selector — icon-first with
// tooltips; applets get placement and duplicate/remove actions. With no
// selection it shows one quiet hint instead of empty form chrome.
T.ScrollView {
    id: root

    objectName: "customizeProperties"
    required property var customizeSettings
    clip: true
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Selected item properties")

    // Matching card chrome so the inspector reads as a pane beside the
    // palette instead of floating text on the page background.
    background: Rectangle {
        color: Tokens.bg.raised
        radius: Tokens.radius.m
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.divider
    }
    leftPadding: Tokens.space["1"]
    rightPadding: Tokens.space["1"]
    topPadding: Tokens.space["1"]
    bottomPadding: Tokens.space["1"]

    readonly property var properties: root.customizeSettings.selectedProperties
    readonly property bool panelSelected: root.properties.kind === "panel"
    readonly property bool appletSelected: root.properties.kind === "applet"

    ColumnLayout {
        width: root.availableWidth
        spacing: Tokens.space["2"]

        // One quiet empty state, sized for its content: the glyph and hint
        // center inside a fixed-height block so neither can clip out of the
        // scrolling card.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            visible: !root.panelSelected && !root.appletSelected

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Tokens.space["3"]
                width: Math.min(parent.width, 200)

                CustomizeAppletIcon {
                    Layout.alignment: Qt.AlignHCenter
                    pluginId: "command-palette"
                    iconSize: 32
                    opacity: 0.65
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Click a panel or applet in the preview to edit it")
                    muted: true
                    font.pointSize: Tokens.type.caption
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    Accessible.name: text
                }
            }
        }

        CustomizePanelPositionProperties {
            Layout.fillWidth: true
            visible: root.panelSelected
            customizeSettings: root.customizeSettings
            properties: root.properties
        }
        CustomizePanelSizeProperties {
            Layout.fillWidth: true
            visible: root.panelSelected
            customizeSettings: root.customizeSettings
            properties: root.properties
        }
        CustomizePanelVisibilityProperties {
            Layout.fillWidth: true
            visible: root.panelSelected
            customizeSettings: root.customizeSettings
            properties: root.properties
        }
        CustomizeAppletProperties {
            Layout.fillWidth: true
            visible: root.appletSelected
            customizeSettings: root.customizeSettings
            properties: root.properties
        }
    }
}
