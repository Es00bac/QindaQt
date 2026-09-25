// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Commander style's function-key bar (ADR-0271), along the bottom of the
// window: F3 View, F4 Edit, F5 Copy, F6 Move, F7 New Folder, F8 Delete. The
// keys themselves work while focus is in a pane (FolderPanes); a button
// runs the same catalog action through the coordinator.
ToolBar {
    id: root
    objectName: "functionKeyBar"

    // FolderPanes.functionKeys: [{key, label, text, action}].
    required property var functionKeys
    required property var appCoordinator

    position: ToolBar.Footer
    padding: 2

    RowLayout {
        anchors.fill: parent
        spacing: 2

        Repeater {
            model: root.functionKeys

            Button {
                id: keyButton
                required property var modelData
                objectName: "functionKey_" + modelData.label
                Layout.fillWidth: true
                flat: true
                focusPolicy: Qt.NoFocus
                text: modelData.label + " " + modelData.text
                Accessible.name: modelData.text
                Accessible.description: qsTr("Function key %1").arg(modelData.label)
                onClicked: root.appCoordinator.activateAction(modelData.action)
            }
        }
    }
}
