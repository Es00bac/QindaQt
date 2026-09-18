// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

// Floating Done / Undo bar shown on every panel while edit mode is active.
// It lives inside the panel surface (layer-shell panels never take keyboard
// focus, so a separate focusable window is not an option); Done exits edit
// mode, Undo walks the engine history one step and applies.
Row {
    id: root
    objectName: "panelEditBar"

    property var controller: null
    readonly property bool editMode: controller !== null && controller.editMode === true

    visible: editMode
    spacing: 4
    z: 200

    T.Button {
        objectName: "panelEditUndo"
        text: qsTr("Undo")
        height: root.height
        enabled: root.controller !== null && root.controller.canUndo === true
        onClicked: root.controller.undo()
    }
    T.Button {
        objectName: "panelEditDone"
        text: qsTr("Done")
        height: root.height
        highlighted: true
        onClicked: root.controller.exitEditMode()
    }
}
