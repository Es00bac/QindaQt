// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T

// Edit-mode bar shown on every panel while edit mode is active (ADR-0266):
// Add applet… (this panel's catalog picker), Undo and Done. It lives inside
// the panel surface at the trailing end of the painted material, in a
// stretch the zones leave free for it, so it never covers an applet. Done
// (or Escape on a panel) leaves edit mode; Undo walks the engine history one
// step and applies.
Grid {
    id: root
    objectName: "panelEditBar"

    property var controller: null
    property string panelId: ""
    // Vertical panels stack full-width buttons; horizontal ones size to text.
    property bool vertical: false
    property real buttonHeight: 24
    property real buttonWidth: -1
    readonly property bool editMode: controller !== null && controller.editMode === true

    visible: editMode
    spacing: 4
    z: 200
    columns: vertical ? 1 : -1
    rows: vertical ? -1 : 1

    // The zone names as a panel's orientation reads them.
    function zoneTitle(zone) {
        if (zone === "start") {
            return vertical ? qsTr("Top") : qsTr("Left")
        }
        if (zone === "end") {
            return vertical ? qsTr("Bottom") : qsTr("Right")
        }
        return vertical ? qsTr("Middle") : qsTr("Center")
    }

    // AGENT-GUARD: the Basic style's content ListView turns `interactive` (and
    // its own key navigation) on whenever its content is a fraction taller
    // than the popup, which makes one Down step twice; pin every menu here.
    function pinKeyboard(menu) {
        if (menu.contentItem) {
            menu.contentItem.interactive = false
            menu.contentItem.keyNavigationEnabled = false
        }
    }

    T.Button {
        id: addButton
        objectName: "panelEditAddApplet"
        text: qsTr("Add applet…")
        height: root.buttonHeight
        width: root.buttonWidth > 0 ? root.buttonWidth : implicitWidth
        enabled: root.controller !== null && root.controller.available === true
        onClicked: picker.popup(addButton, 0, addButton.height)
    }
    T.Button {
        objectName: "panelEditUndo"
        text: qsTr("Undo")
        height: root.buttonHeight
        width: root.buttonWidth > 0 ? root.buttonWidth : implicitWidth
        enabled: root.controller !== null && root.controller.canUndo === true
        onClicked: root.controller.undo()
    }
    T.Button {
        objectName: "panelEditDone"
        text: qsTr("Done")
        height: root.buttonHeight
        width: root.buttonWidth > 0 ? root.buttonWidth : implicitWidth
        highlighted: true
        onClicked: root.controller.exitEditMode()
    }

    // The applet picker: one submenu per zone of this panel, each listing
    // what the applet catalog admits there (the controller's palette rows for
    // this panel's orientation). Choosing one appends it to that zone through
    // the editor's InsertApplet intent: one gesture and one Apply, with the
    // same undo, persistence and escrow as every other edit.
    T.Menu {
        id: picker
        objectName: "panelAppletPicker"
        popupType: T.Popup.Window
        property var rows: []

        onAboutToShow: rows = root.controller !== null ? root.controller.palette(root.panelId) : []
        // Keyboard contract: a freshly opened menu has no current entry.
        onOpened: currentIndex = -1
        Component.onCompleted: root.pinKeyboard(this)

        Instantiator {
            model: ["start", "center", "end"]
            delegate: T.Menu {
                id: zoneMenu
                required property string modelData
                readonly property var zoneRows: picker.rows.filter(
                    row => Array.from(row.zones ?? []).map(String).includes(zoneMenu.modelData))

                objectName: "panelAppletPicker:" + modelData
                title: root.zoneTitle(modelData)
                enabled: zoneRows.length > 0
                Component.onCompleted: root.pinKeyboard(this)

                Instantiator {
                    model: zoneMenu.zoneRows
                    delegate: T.MenuItem {
                        required property var modelData
                        objectName: "panelAppletPicker:" + zoneMenu.modelData + ":"
                                    + String(modelData.pluginId)
                        text: String(modelData.name)
                        onTriggered: root.controller.addApplet(root.panelId, zoneMenu.modelData,
                                                               String(modelData.pluginId))
                    }
                    onObjectAdded: (index, object) => zoneMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => zoneMenu.removeItem(object)
                }
            }
            onObjectAdded: (index, object) => picker.insertMenu(index, object)
            onObjectRemoved: (index, object) => picker.removeMenu(object)
        }
    }
}
