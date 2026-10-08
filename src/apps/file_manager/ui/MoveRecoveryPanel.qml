// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var mutationController
    property var pendingRestore: null
    readonly property var records: mutationController.recoveryRecords || []
    readonly property bool supportsRecovery: typeof mutationController.inspectRecovery === "function"
    property bool initialInspectionRequested: false
    spacing: 4
    visible: supportsRecovery

    function inspectOnce() {
        if (!initialInspectionRequested && supportsRecovery && !mutationController.busy) {
            initialInspectionRequested = true
            mutationController.inspectRecovery()
        }
    }
    Component.onCompleted: inspectOnce()
    Connections {
        target: root.mutationController
        ignoreUnknownSignals: true
        function onStateChanged() { root.inspectOnce() }
    }

    RowLayout {
        Layout.fillWidth: true
        Label {
            Layout.fillWidth: true
            text: qsTr("Moving between volumes retains originals for recovery and does not free their space.")
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            Accessible.name: text
        }
        Button {
            objectName: "inspectMoveRecoveryButton"
            text: qsTr("Inspect recovery")
            enabled: !root.mutationController.busy
            onClicked: root.mutationController.inspectRecovery()
        }
    }
    Repeater {
        model: root.records
        delegate: StatusBanner {
            required property var modelData
            Layout.fillWidth: true
            objectName: "moveRecoveryReceipt"
            title: modelData.uncertain ? qsTr("Recovery requires inspection") : qsTr("Move recovery")
            message: qsTr("Phase: %1; retained byte estimate: %2\nOriginal: %3\nDestination: %4\nRecovery: %5\nStage: %6")
                .arg(modelData.phase).arg(modelData.retainedBytes)
                .arg(modelData.sourcePath).arg(modelData.destinationPath)
                .arg(modelData.recoveryDirectory).arg(modelData.stageDirectory)
            actionText: modelData.canRestore && !root.mutationController.busy
                ? qsTr("Restore retained source…") : ""
            onActionTriggered: {
                root.pendingRestore = modelData
                confirmation.open()
            }
        }
    }
    property MoveRecoveryRestoreDialog restoreConfirmation: MoveRecoveryRestoreDialog {
        id: confirmation
        receipt: root.pendingRestore
        onRestoreRequested: operationId => {
            root.mutationController.restoreRecovery(operationId)
            root.pendingRestore = null
        }
        onRejected: root.pendingRestore = null
    }
}
