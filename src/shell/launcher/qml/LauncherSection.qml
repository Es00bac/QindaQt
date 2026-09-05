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
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                }
                Label {
                    visible: row.modelData.pinned
                    text: qsTr("Pinned")
                    muted: true
                }

                Button {
                    objectName: "launcherTogglePinButton"
                    text: row.modelData.pinned ? qsTr("Unpin") : qsTr("Pin")
                    emphasized: false
                    implicitWidth: Math.max(56, implicitContentWidth
                                            + Tokens.space["2"] * 2)
                    implicitHeight: Math.max(28, implicitContentHeight
                                             + Tokens.space["1"] * 2)
                    leftPadding: Tokens.space["2"]
                    rightPadding: Tokens.space["2"]
                    topPadding: Tokens.space["1"]
                    bottomPadding: Tokens.space["1"]
                    accessibleDescription: row.modelData.pinned
                        ? qsTr("Remove this application from Quick Launch")
                        : qsTr("Add this application to Quick Launch")
                    onClicked: {
                        if (row.modelData.pinned)
                            root.controller.unpin(row.modelData.entryId)
                        else
                            root.controller.pin(row.modelData.entryId)
                    }
                }
            }
        }
    }

}
