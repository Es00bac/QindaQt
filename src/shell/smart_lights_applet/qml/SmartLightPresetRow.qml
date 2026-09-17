// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

Frame {
    id: root

    required property var row
    required property var access
    required property var colors

    objectName: "smartLightPresetRow"

    RowLayout {
        anchors.fill: parent
        spacing: 8

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: root.row.name
                color: root.colors.text ?? "white"
                elide: Text.ElideRight
                Accessible.name: root.row.accessibleName
                Accessible.description: root.row.accessibleDescription
            }

            Label {
                Layout.fillWidth: true
                text: root.row.summary
                color: root.colors.textMuted ?? "#a9afa9"
                elide: Text.ElideRight
            }
        }

        C.Button {
            objectName: "smartLightPresetApplyButton"
            emphasized: false
            text: qsTr("Apply")
            available: root.row.applicable
            accessibleDescription: root.row.accessibleDescription
            onClicked: root.access.applyPreset(root.row.presetId)
        }

        C.Button {
            objectName: "smartLightPresetDeleteButton"
            emphasized: false
            destructive: true
            text: qsTr("Delete")
            accessibleDescription: qsTr("Delete the %1 arrangement").arg(root.row.name)
            onClicked: root.access.deletePreset(root.row.presetId)
        }
    }
}
