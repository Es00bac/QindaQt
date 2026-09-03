// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

FormSurface {
    id: root
    required property var customizeSettings
    required property var properties

    ColumnLayout {
        width: parent.width
        spacing: Tokens.space["2"]
        Label {
            Layout.fillWidth: true
            text: qsTr("Zone: %1").arg(root.properties.zone ?? "start")
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Manifest settings")
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
        Repeater {
            model: root.properties.settingsFields ?? []
            delegate: Label {
                required property var modelData
                Layout.fillWidth: true
                text: qsTr("%1: %2").arg(modelData.label).arg(modelData.value)
                wrapMode: Text.Wrap
                Accessible.role: Accessible.StaticText
                Accessible.name: text
                Accessible.description: qsTr("Read-only schema field of type %1").arg(modelData.type)
            }
        }
        Label {
            Layout.fillWidth: true
            visible: (root.properties.settingsFields ?? []).length === 0
            text: root.properties.schemaAvailable
                  ? qsTr("This applet has no configurable manifest fields")
                  : qsTr("Applet manifest unavailable; placement changes fail closed")
            wrapMode: Text.Wrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                objectName: "customizeDuplicateButton"
                text: qsTr("Duplicate")
                available: root.customizeSettings.canEdit
                emphasized: false
                onClicked: root.customizeSettings.duplicateSelected()
            }
            Button {
                objectName: "customizeRemoveButton"
                text: qsTr("Remove")
                destructive: true
                available: root.customizeSettings.canEdit
                onClicked: root.customizeSettings.removeSelected()
            }
        }
    }
}
