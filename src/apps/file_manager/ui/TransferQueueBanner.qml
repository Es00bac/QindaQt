// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The visible half of TransferQueueController (ADR-0195): what is running,
// how far along, what is still waiting, and what failed. Presentation only --
// every decision (order, pause, cancel) is the controller's.
Control {
    id: root
    objectName: "transferQueueBanner"

    required property var transferQueueController

    visible: root.transferQueueController.busy
             || root.transferQueueController.failedCount > 0

    padding: 6
    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.transferQueueController.busy
        ? qsTr("%1. %2 percent. %3 waiting.")
              .arg(root.transferQueueController.activeDescription)
              .arg(root.transferQueueController.activePercent)
              .arg(root.transferQueueController.queuedCount)
        : qsTr("%1 transfers failed").arg(root.transferQueueController.failedCount)

    background: Rectangle {
        radius: 4
        color: root.palette.alternateBase
        border.color: root.palette.mid
    }

    contentItem: ColumnLayout {
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                objectName: "transferQueueDescription"
                Layout.fillWidth: true
                text: root.transferQueueController.busy
                    ? (root.transferQueueController.queuedCount > 0
                       ? qsTr("%1 (%2 waiting)")
                             .arg(root.transferQueueController.activeDescription)
                             .arg(root.transferQueueController.queuedCount)
                       : root.transferQueueController.activeDescription)
                    : qsTr("%1 transfers failed").arg(root.transferQueueController.failedCount)
                elide: Text.ElideMiddle
                Accessible.ignored: true
            }
            Button {
                objectName: "transferQueuePauseButton"
                visible: root.transferQueueController.busy
                text: root.transferQueueController.activePaused
                    ? qsTr("Resume") : qsTr("Pause")
                onClicked: root.transferQueueController.activePaused
                    ? root.transferQueueController.resumeActive()
                    : root.transferQueueController.pauseActive()
            }
            Button {
                objectName: "transferQueueCancelButton"
                visible: root.transferQueueController.busy
                text: qsTr("Cancel all")
                onClicked: root.transferQueueController.cancelAll()
            }
            Button {
                objectName: "transferQueueClearButton"
                visible: !root.transferQueueController.busy
                text: qsTr("Dismiss")
                onClicked: root.transferQueueController.clearFinished()
            }
        }

        ProgressBar {
            objectName: "transferQueueProgress"
            Layout.fillWidth: true
            visible: root.transferQueueController.busy
            from: 0
            to: 100
            value: root.transferQueueController.activePercent
            Accessible.ignored: true
        }
    }
}
