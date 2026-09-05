// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// MATE-style Places menu: standard user folders opened in the QindaQt File
// Manager through the places facade's bounded process seam.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready
    readonly property var rows: ready ? access.rows : []

    objectName: "placesMenuApplet"
    implicitWidth: button.implicitWidth
    implicitHeight: 28

    function focusRow(index) {
        if (index < 0 || index >= rowRepeater.count)
            return
        const item = rowRepeater.itemAt(index)
        if (item !== null)
            item.forceActiveFocus(Qt.TabFocusReason)
    }

    DesktopControlButton {
        id: button
        objectName: "placesMenuButton"
        anchors.fill: parent
        iconName: "folder"
        fallbackText: qsTr("Places")
        labelText: qsTr("Places")
        showLabel: true
        vertical: root.vertical
        available: root.ready && Boolean(root.access.available)
        active: menu.opened
        Accessible.name: qsTr("Places")
        accessibleDescription: available
                               ? qsTr("Opens a folder in the file manager")
                               : qsTr("The file manager is unavailable")
        onTriggered: if (available) menu.open()
    }

    ControlPopupFrame {
        id: menu
        objectName: "placesMenuPopup"
        heading: qsTr("Places")
        feedback: root.ready && root.access.feedbackPresent ? String(root.access.feedback) : ""
        initialFocusItem: rowRepeater.count > 0 ? rowRepeater.itemAt(0) : null
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)

        Repeater {
            id: rowRepeater
            model: root.rows

            MenuRow {
                required property var modelData
                required property int index

                objectName: "placesMenuRow"
                Layout.fillWidth: true
                iconName: String(modelData.iconName)
                text: String(modelData.label)
                detail: String(modelData.path)
                Accessible.name: String(modelData.accessibleName)
                Keys.onUpPressed: root.focusRow(index - 1)
                Keys.onDownPressed: root.focusRow(index + 1)
                onActivated: if (root.access.open(String(modelData.id))) menu.close()
            }
        }
    }
}
