// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The one Trash preference (ADR-0198). Only the recoverable home Trash may
// skip its confirmation; Empty Trash is permanent and always confirms, which
// is why there is no knob for it here.
ColumnLayout {
    id: root
    objectName: "preferencesTrashPage"

    required property var preferencesController

    spacing: 10

    CheckBox {
        objectName: "preferenceConfirmTrashBox"
        text: qsTr("Ask before moving items to Trash")
        checked: root.preferencesController.confirmTrash
        Accessible.name: text
        onToggled: root.preferencesController.setConfirmTrash(checked)
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Items in the Trash can be restored. Emptying the Trash is "
                 + "permanent and always asks, whatever this is set to.")
        color: root.palette.placeholderText
        wrapMode: Text.WordWrap
        Accessible.ignored: true
    }

    Item { Layout.fillHeight: true }
}
