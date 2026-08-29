// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Bounded clipboard entry row presentation component.
// Dispatches user intents (select/promote, delete, togglePin) to the controller facade.
T.Control {
    id: rowRoot

    required property var entry
    required property var controller

    property bool isCurrent: false
    // Honest capability input from the applet surface: mutating intents are
    // offerable only in the ready phase (a degraded service refuses them, so
    // dead controls would mislead users). Read-only browsing stays available.
    property bool actionsAvailable: true

    signal selectRequested()
    signal deleteRequested()
    signal togglePinRequested()

    Accessible.role: Accessible.ListItem
    Accessible.name: entry ? entry.accessibleName : ""
    Accessible.description: entry ? entry.accessibleDescription : ""

    implicitWidth: parent ? parent.width : 340
    implicitHeight: Math.max(56, contentLayout.implicitHeight + topPadding + bottomPadding)

    leftPadding: Tokens.space["3"]
    rightPadding: Tokens.space["3"]
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["2"]

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Keys.onReturnPressed: {
        if (entry && controller) {
            controller.selectEntry(entry.generation, entry.serial)
        }
    }
    Keys.onEnterPressed: Keys.onReturnPressed(event)
    Keys.onDeletePressed: {
        if (entry && controller) {
            controller.deleteEntry(entry.generation, entry.serial)
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: rowRoot.down ? Tokens.state.pressed
             : rowRoot.hovered || rowRoot.isCurrent ? Tokens.state.hover
             : Tokens.bg.raised
        border.width: rowRoot.activeFocus ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: rowRoot.activeFocus ? Tokens.focus.ring
                     : entry?.pinned ? Tokens.accent.default
                     : Tokens.outline.subtle

        Behavior on color {
            ColorAnimation { duration: Tokens.motion.short }
        }
        Behavior on border.color {
            ColorAnimation { duration: Tokens.motion.short }
        }
    }

    contentItem: RowLayout {
        id: contentLayout
        spacing: Tokens.space["3"]

        // Media Type Badge
        Rectangle {
            id: typeBadge
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter
            radius: Tokens.radius.s
            color: entry?.pinned ? Tokens.accent.default : Tokens.bg.highest
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.outline.strong

            Text {
                anchors.centerIn: parent
                text: {
                    if (!rowRoot.entry) return ""
                    if (rowRoot.entry.isImage) return qsTr("IMG")
                    if (rowRoot.entry.isUriList) return qsTr("URI")
                    if (rowRoot.entry.isText) return qsTr("TXT")
                    return qsTr("BIN")
                }
                color: entry?.pinned ? Tokens.accent.fg : Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                font.weight: Font.Bold
            }
        }

        // Preview & Metadata Column
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Tokens.space["1"]

            Text {
                id: previewText
                Layout.fillWidth: true
                text: rowRoot.entry ? (rowRoot.entry.preview.length > 0 ? rowRoot.entry.preview : qsTr("(Empty content)")) : ""
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: Font.Medium
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WrapAnywhere
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["2"]

                Text {
                    id: summaryText
                    Layout.fillWidth: true
                    text: rowRoot.entry ? rowRoot.entry.formatsSummary : ""
                    color: Tokens.fg.muted
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    elide: Text.ElideRight
                }

                Text {
                    id: sourceLabelText
                    visible: rowRoot.entry && rowRoot.entry.sourceLabel.length > 0
                    text: rowRoot.entry ? rowRoot.entry.sourceLabel : ""
                    color: Tokens.fg.subtle
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    elide: Text.ElideRight
                }
            }
        }

        // Action Buttons Row
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: Tokens.space["2"]

            // Pin / Unpin Button
            C.Button {
                id: pinButton
                objectName: "pinButton"
                text: entry?.pinned ? qsTr("Unpin") : qsTr("Pin")
                emphasized: entry?.pinned ?? false
                // Pending presentation: in-flight mutations show the
                // Controls busy state (disabled + "Working…") until the
                // completion lands.
                busy: entry?.pending ?? false
                available: rowRoot.actionsAvailable
                implicitHeight: 32
                implicitWidth: 64
                accessibleDescription: entry?.pinned ? qsTr("Unpin entry") : qsTr("Pin entry")
                onClicked: {
                    if (rowRoot.entry && rowRoot.controller) {
                        rowRoot.controller.togglePin(rowRoot.entry.generation, rowRoot.serial)
                    }
                }
            }

            // Delete Button
            C.Button {
                id: deleteButton
                objectName: "deleteButton"
                text: qsTr("Delete")
                destructive: true
                emphasized: false
                busy: entry?.pending ?? false
                available: rowRoot.actionsAvailable
                implicitHeight: 32
                implicitWidth: 64
                accessibleDescription: qsTr("Delete this clipboard entry")
                onClicked: {
                    if (rowRoot.entry && rowRoot.controller) {
                        rowRoot.controller.deleteEntry(rowRoot.entry.generation, rowRoot.entry.serial)
                    }
                }
            }
        }
    }

    // AGENT-GUARD: row-body selection must sit BELOW the content item. A
    // default-stacked MouseArea declared last covers the action buttons and
    // swallows their pointer events, which is exactly the P1 defect: real
    // clicks on Pin/Delete never arrived. z: -1 keeps the buttons (real
    // input-handling items) on top while clicks elsewhere fall through.
    MouseArea {
        id: clickArea
        objectName: "rowClickArea"
        anchors.fill: parent
        z: -1
        enabled: rowRoot.actionsAvailable
        acceptedButtons: Qt.LeftButton
        onClicked: {
            if (rowRoot.entry && rowRoot.controller) {
                rowRoot.controller.selectEntry(rowRoot.entry.generation, rowRoot.entry.serial)
            }
        }
    }
}
