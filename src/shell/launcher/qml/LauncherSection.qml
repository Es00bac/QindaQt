// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls as C
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One launcher section: a translated header plus its result rows. Keyboard
// traversal is flat Up/Down across the rows of all sections (the controller's
// section order is the focus order); cross-section moves are relayed to the
// applet root through flatFocusRequested.
ColumnLayout {
    id: root

    required property var section
    required property var controller
    property bool vertical: false
    property int flatBase: 0
    property string contextEntryId: ""
    property bool contextEntryPinned: false
    readonly property int rowCount: section && section.items
                                    ? section.items.length : 0

    // Translated from the stable identity the controller publishes; the L0
    // model owns no user-facing strings.
    function sectionTitle(identity) {
        switch (identity) {
        case "pinned": return qsTr("Pinned")
        case "recent": return qsTr("Recent")
        case "searchResults": return qsTr("Search results")
        case "utilities": return qsTr("Utilities")
        case "development": return qsTr("Development")
        case "education": return qsTr("Education")
        case "games": return qsTr("Games")
        case "graphics": return qsTr("Graphics")
        case "audioVideo": return qsTr("Audio & Video")
        case "network": return qsTr("Network")
        case "office": return qsTr("Office")
        case "science": return qsTr("Science")
        case "settings": return qsTr("Settings")
        case "system": return qsTr("System")
        default: return qsTr("Other")
        }
    }

    function focusRowAt(flatIndex) {
        if (flatIndex < flatBase || flatIndex >= flatBase + rowCount)
            return false
        const row = rows.itemAt(flatIndex - flatBase)
        if (!row)
            return false
        row.forceActiveFocus()
        return true
    }

    signal flatFocusRequested(int flatIndex)

    spacing: Tokens.space["1"]

    SectionHeader {
        objectName: "launcherSectionHeader-"
                    + (root.section ? root.section.identity : "")
        Layout.fillWidth: true
        visible: root.rowCount > 0
        title: root.section
               ? root.sectionTitle(root.section.identity) : ""
    }

    Repeater {
        id: rows

        model: root.section && root.section.items ? root.section.items : []

        T.ItemDelegate {
            id: row

            required property var modelData
            required property int index
            readonly property int flatIndex: root.flatBase + index

            objectName: "launcherResultRow-" + modelData.entryId
                        + "-" + flatIndex
            Layout.fillWidth: true
            text: modelData.displayText
            enabled: root.controller !== null && root.controller.launchGranted
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            leftPadding: Tokens.space["3"]
            rightPadding: Tokens.space["3"]
            topPadding: Tokens.space["2"]
            bottomPadding: Tokens.space["2"]
            implicitHeight: Math.max(32, implicitContentHeight + topPadding + bottomPadding)
            Accessible.role: Accessible.ListItem
            Accessible.name: modelData.displayText
            Accessible.description: modelData.accessibleDescription

            function launch() {
                if (root.controller !== null)
                    root.controller.activate(modelData.entryId, "")
            }

            onClicked: launch()
            Keys.onReturnPressed: launch()
            Keys.onEnterPressed: launch()
            Keys.onSpacePressed: launch()
            Keys.onUpPressed: root.flatFocusRequested(flatIndex - 1)
            Keys.onDownPressed: root.flatFocusRequested(flatIndex + 1)
            Accessible.onPressAction: launch()

            // Keep pinning on an explicit secondary action: primary click and
            // Enter remain the launcher contract for activation.
            // Use the same full-row secondary hit target as task entries and
            // Quick Launch. The nested-session compositor delivers a real
            // BTN_RIGHT click to MouseArea; a TapHandler nested in the native
            // ItemDelegate did not receive that gesture even though the
            // offscreen QTest path synthesized it successfully.
            MouseArea {
                acceptedButtons: Qt.RightButton
                anchors.fill: parent
                onClicked: {
                    root.contextEntryId = row.modelData.entryId
                    root.contextEntryPinned = row.modelData.pinned
                    pinMenu.popup()
                }
            }

            Keys.onPressed: function(event) {
                if (event.key !== Qt.Key_Menu
                        && !(event.key === Qt.Key_F10
                             && (event.modifiers & Qt.ShiftModifier)))
                    return
                root.contextEntryId = row.modelData.entryId
                root.contextEntryPinned = row.modelData.pinned
                pinMenu.popup(row)
                event.accepted = true
            }

            // AGENT-GUARD: native delegate styles can paint a light surface
            // behind token-colored text. Own both sides of the contrast pair.
            background: Rectangle {
                objectName: "launcherResultBackground"
                color: Tokens.bg.raised
                radius: Tokens.radius.m
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: row.down ? Tokens.state.pressed
                         : row.hovered ? Tokens.state.hover : "transparent"
                    Accessible.ignored: true
                }
                FocusRing {
                    anchors.fill: parent
                    control: row
                }
            }

            contentItem: RowLayout {
                spacing: Tokens.space["2"]

                Label {
                    objectName: "launcherResultText"
                    Layout.fillWidth: true
                    text: row.modelData.displayText
                    elide: Text.ElideRight
                }
                Label {
                    visible: row.modelData.pinned
                    text: qsTr("Pinned")
                    muted: true
                }
            }
        }
    }

    // The controller is the only persistence authority. This menu carries an
    // identity supplied by its row and never invents a pinned projection.
    C.Menu {
        id: pinMenu
        objectName: "launcherPinContextMenu"
        popupType: C.Popup.Window

        C.MenuItem {
            objectName: "launcherTogglePin"
            text: root.contextEntryPinned ? qsTr("Unpin") : qsTr("Pin")
            enabled: root.controller !== null && root.contextEntryId.length > 0
            onTriggered: {
                if (root.contextEntryPinned)
                    root.controller.unpin(root.contextEntryId)
                else
                    root.controller.pin(root.contextEntryId)
            }
        }
    }
}
