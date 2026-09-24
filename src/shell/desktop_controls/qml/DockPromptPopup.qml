// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// The dock's one small question (ADR-0265): name a new group, rename a
// group, or confirm emptying the Trash (which deletes permanently, so it is
// never a single unconfirmed click). It reports the answer through
// `confirmed`; the applet makes the facade call.
ControlPopupFrame {
    id: prompt

    // "newGroup" | "rename" | "emptyTrash"
    property string mode: "rename"
    // The dock index the answer applies to.
    property int targetIndex: -1

    readonly property bool asksName: mode !== "emptyTrash"

    signal confirmed(string mode, int targetIndex, string text)

    function ask(nextMode, index, text, anchor) {
        mode = nextMode
        targetIndex = index
        nameField.text = text
        anchorItem = anchor
        open()
        if (asksName)
            nameField.selectAll()
    }

    function commit() {
        const text = nameField.text.trim()
        if (asksName && text.length === 0)
            return
        close()
        confirmed(mode, targetIndex, text)
    }

    objectName: "quickLaunchPromptPopup"
    heading: mode === "emptyTrash" ? qsTr("Empty the Trash?")
           : mode === "newGroup" ? qsTr("New Group") : qsTr("Rename Group")
    initialFocusItem: asksName ? nameField : confirmButton

    C.Label {
        objectName: "quickLaunchPromptWarning"
        Layout.fillWidth: true
        Layout.maximumWidth: 320
        visible: !prompt.asksName
        text: qsTr("Everything in the Trash is deleted permanently. This cannot be undone.")
        wrapMode: Text.WordWrap
    }

    C.TextField {
        id: nameField
        objectName: "quickLaunchPromptName"
        Layout.fillWidth: true
        Layout.minimumWidth: 240
        visible: prompt.asksName
        accessibleName: qsTr("Group name")
        // The stored bound lives in the dock codec; longer names are cut
        // there, never here, so the limit has one owner.
        onAccepted: prompt.commit()
    }

    RowLayout {
        Layout.alignment: Qt.AlignRight
        spacing: Tokens.space["2"]

        C.Button {
            objectName: "quickLaunchPromptCancel"
            text: qsTr("Cancel")
            emphasized: false
            onClicked: prompt.close()
        }
        C.Button {
            id: confirmButton
            objectName: "quickLaunchPromptConfirm"
            text: prompt.asksName ? qsTr("OK") : qsTr("Empty Trash")
            destructive: !prompt.asksName
            enabled: !prompt.asksName || nameField.text.trim().length > 0
            onClicked: prompt.commit()
        }
    }
}
