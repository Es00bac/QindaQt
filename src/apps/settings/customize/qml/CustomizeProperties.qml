// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.ScrollView {
    id: root
    objectName: "customizeProperties"
    required property var customizeSettings
    clip: true
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Selected item properties")

    readonly property var properties: root.customizeSettings.selectedProperties
    readonly property bool panelSelected: root.properties.kind === "panel"
    readonly property bool appletSelected: root.properties.kind === "applet"

    ColumnLayout {
        width: root.availableWidth
        spacing: Tokens.space["2"]

        SectionHeader {
            Layout.fillWidth: true
            title: root.properties.name ?? qsTr("Properties")
            description: root.panelSelected
                         ? qsTr("Panel position, size, and visibility")
                         : root.appletSelected
                           ? qsTr("Applet placement and manifest settings")
                           : qsTr("Select a panel or applet in the layout outline")
        }

        Label {
            Layout.fillWidth: true
            visible: !root.panelSelected && !root.appletSelected
            text: qsTr("Nothing selected")
            wrapMode: Text.Wrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
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
