// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ADR-0116: stock Qt Quick Controls only. All state comes from the injected
// EntryPropertiesController; this dialog owns no filesystem policy.
Dialog {
    id: root

    required property var controller

    title: qsTr("Properties — %1").arg(controller.name)
    modal: true
    standardButtons: Dialog.Close
    anchors.centerIn: parent
    width: Math.min(420, parent ? parent.width - 48 : 420)

    onClosed: controller.clear()

    GridLayout {
        anchors.fill: parent
        columns: 2
        columnSpacing: 12
        rowSpacing: 6

        Label { text: qsTr("Name:"); font.bold: true }
        Label {
            text: root.controller.name
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            Accessible.ignored: true
        }

        Label { text: qsTr("Kind:"); font.bold: true; visible: root.controller.kindText.length > 0 }
        Label {
            text: root.controller.kindText
            visible: root.controller.kindText.length > 0
            Layout.fillWidth: true
            Accessible.ignored: true
        }

        Label { text: qsTr("Type:"); font.bold: true; visible: root.controller.mimeText.length > 0 }
        Label {
            text: root.controller.mimeText
            visible: root.controller.mimeText.length > 0
            Layout.fillWidth: true
            Accessible.ignored: true
        }

        Label {
            text: qsTr("Size:")
            font.bold: true
            visible: root.controller.sizeText.length > 0
        }
        Label {
            text: root.controller.sizeText
            visible: root.controller.sizeText.length > 0
            Accessible.ignored: true
        }

        Label {
            text: qsTr("Total size:")
            font.bold: true
            visible: root.controller.computingTotal || root.controller.totalSizeText.length > 0
        }
        Label {
            visible: root.controller.computingTotal || root.controller.totalSizeText.length > 0
            text: root.controller.computingTotal
                ? qsTr("Calculating…")
                : root.controller.totalTruncated
                    ? qsTr("%1 (partial, limit reached)").arg(root.controller.totalSizeText)
                    : root.controller.totalSizeText
            Accessible.ignored: true
        }

        Label { text: qsTr("Modified:"); font.bold: true; visible: root.controller.modifiedText.length > 0 }
        Label {
            text: root.controller.modifiedText
            visible: root.controller.modifiedText.length > 0
            Layout.fillWidth: true
            Accessible.ignored: true
        }

        Label { text: qsTr("Permissions:"); font.bold: true; visible: root.controller.permissionsText.length > 0 }
        Label {
            text: root.controller.permissionsText
            visible: root.controller.permissionsText.length > 0
            Accessible.ignored: true
        }

        Label { text: qsTr("Location:"); font.bold: true; visible: root.controller.pathText.length > 0 }
        Label {
            text: root.controller.pathText
            visible: root.controller.pathText.length > 0
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            Accessible.ignored: true
        }
    }
}
