// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One layout preset: a miniature desktop rendered from the preset's panel
// summary (edge, alignment, length) plus its name, text badges for Default
// and Modified, and the actions this kind of preset allows (ADR-0267).
// Choosing a layout is a visual decision; the tooltip carries the catalog
// description. A pure presentation delegate: every action leaves through
// actionRequested and the page decides what it means.
Column {
    id: card

    required property var profile
    required property bool selected
    required property bool available
    property bool actionsAvailable: available
    property int miniatureWidth: 144
    readonly property int miniatureHeight: miniatureWidth * 9 / 16
    readonly property var panelSummaries: profile.panels ?? []
    readonly property string presetId: String(profile.id ?? "")
    readonly property string presetName: String(profile.name ?? "")
    readonly property bool own: profile.own === true
    readonly property bool modified: profile.modified === true
    readonly property bool isDefault: profile.isDefault === true
    readonly property alias selectButton: selectControl

    // "activate", "rename", "duplicate", "delete", "restore" or "saveAs".
    signal actionRequested(string action)

    width: miniatureWidth
    spacing: Tokens.space["1"]

    component Badge: Rectangle {
        id: badge

        property string text: ""

        implicitWidth: badgeLabel.implicitWidth + Tokens.space["3"]
        implicitHeight: badgeLabel.implicitHeight + Tokens.space["1"]
        radius: height / 2
        color: Tokens.bg.highest
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.strong

        Label {
            id: badgeLabel
            anchors.centerIn: parent
            text: badge.text
            font.pointSize: Tokens.type.caption
            // The same words are in the card's accessible name.
            Accessible.ignored: true
        }
    }

    T.AbstractButton {
        id: selectControl

        objectName: "customizeProfileCard_" + card.presetId
        width: card.miniatureWidth
        implicitHeight: card.miniatureHeight + Tokens.space["3"] + captionMetrics.height
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        enabled: card.available
        padding: 0

        Accessible.role: Accessible.RadioButton
        Accessible.name: card.modified
            ? qsTr("%1 layout preset, modified").arg(card.presetName)
            : qsTr("%1 layout preset").arg(card.presetName)
        Accessible.description: String(card.profile.description ?? "")
        Accessible.checked: card.selected
        T.ToolTip.visible: selectControl.hovered
                           && String(card.profile.description ?? "").length > 0
        T.ToolTip.delay: 500
        T.ToolTip.text: String(card.profile.description ?? "")

        onClicked: card.actionRequested("activate")
        Keys.onReturnPressed: card.actionRequested("activate")
        Keys.onEnterPressed: card.actionRequested("activate")

        TextMetrics {
            id: captionMetrics
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            text: card.presetName
        }

        background: Item {
            Rectangle {
                anchors.fill: parent
                radius: Tokens.radius.m
                color: card.selected ? Tokens.accent.subtle
                     : selectControl.hovered && selectControl.enabled ? Tokens.state.hover
                     : "transparent"
                border.width: card.selected ? Tokens.space["1"] : 0
                border.color: Tokens.accent.default
            }
            FocusRing {
                anchors.fill: parent
                visible: selectControl.activeFocus
                control: selectControl
            }
        }

        contentItem: Column {
            spacing: Tokens.space["2"]

            Rectangle {
                id: miniature

                width: card.miniatureWidth
                height: card.miniatureHeight
                radius: Tokens.radius.s
                color: Tokens.bg.highest
                border.width: Tokens.space["1"] / 2
                border.color: card.selected ? Tokens.accent.default : Tokens.outline.strong

                // Backdrop keeps the vertical light falloff of the live desktop,
                // lifted above the card material so panel bars stay legible.
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: Qt.lighter(Tokens.bg.base, 1.35)
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.lighter(Tokens.bg.base, 1.1)
                        }
                    }
                }

                Repeater {
                    model: card.panelSummaries

                    delegate: Rectangle {
                        required property var modelData

                        readonly property bool horizontal:
                            modelData.edge === "top" || modelData.edge === "bottom"
                        // Card-scale panel bars: proportionally faithful edges
                        // and lengths with a legibility floor.
                        readonly property real barThickness:
                            Math.max(3, Number(modelData.thickness ?? 32)
                                      * card.miniatureWidth / 1920 * 2.2)
                        readonly property real barLength:
                            card.miniatureWidth * Number(modelData.length ?? 1.0)
                        readonly property real crossLength:
                            card.miniatureHeight * Number(modelData.length ?? 1.0)

                        width: horizontal ? barLength : barThickness
                        height: horizontal ? barThickness : crossLength
                        x: modelData.edge === "left" ? 0
                           : modelData.edge === "right" ? miniature.width - width
                           : modelData.alignment === "start" ? 0
                           : modelData.alignment === "end" ? miniature.width - width
                           : (miniature.width - width) / 2
                        y: modelData.edge === "top" ? 0
                           : modelData.edge === "bottom" ? miniature.height - height
                           : modelData.alignment === "start" ? 0
                           : modelData.alignment === "end" ? miniature.height - height
                           : (miniature.height - height) / 2
                        radius: modelData.alignment === "fill" ? 0 : Tokens.radius.s
                        color: card.selected ? Tokens.accent.default : Tokens.fg.muted
                        opacity: modelData.layer === "below" ? 0.8 : 1.0
                    }
                }

                // Selected marker: a badge shape, not only a border colour, so
                // the current layout stays identifiable at a glance.
                Rectangle {
                    visible: card.selected
                    width: Tokens.space["4"]
                    height: width
                    radius: width / 2
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Tokens.space["1"]
                    color: Tokens.accent.default
                    border.width: Tokens.space["1"] / 2
                    border.color: Tokens.bg.base

                    Rectangle {
                        anchors.centerIn: parent
                        width: Tokens.space["1"] * 1.5
                        height: width
                        radius: width / 2
                        color: Tokens.accent.fg
                    }
                }
            }

            Text {
                width: card.miniatureWidth
                text: card.presetName
                color: selectControl.enabled ? Tokens.fg.default : Tokens.fg.disabled
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Flow {
        width: card.miniatureWidth
        spacing: Tokens.space["1"]
        visible: card.isDefault || card.modified

        Badge {
            objectName: "customizePresetDefaultBadge_" + card.presetId
            visible: card.isDefault
            text: qsTr("Default")
        }
        Badge {
            objectName: "customizePresetModifiedBadge_" + card.presetId
            visible: card.modified
            text: qsTr("Modified")
        }
    }

    // Own presets: rename, duplicate, delete. An edited built-in: restore the
    // original, or keep the edits as a new preset.
    Row {
        spacing: Tokens.space["1"]
        visible: card.own || card.modified

        CustomizeIconButton {
            objectName: "customizePresetRename_" + card.presetId
            visible: card.own
            implicitWidth: 32
            implicitHeight: 32
            iconName: "document-edit"
            toolTip: qsTr("Rename “%1”…").arg(card.presetName)
            available: card.actionsAvailable
            onClicked: card.actionRequested("rename")
        }
        CustomizeIconButton {
            objectName: "customizePresetDuplicate_" + card.presetId
            visible: card.own
            implicitWidth: 32
            implicitHeight: 32
            iconName: "edit-copy"
            toolTip: qsTr("Duplicate “%1”").arg(card.presetName)
            available: card.actionsAvailable
            onClicked: card.actionRequested("duplicate")
        }
        CustomizeIconButton {
            objectName: "customizePresetDelete_" + card.presetId
            visible: card.own
            implicitWidth: 32
            implicitHeight: 32
            iconName: "edit-delete"
            destructive: true
            toolTip: qsTr("Delete “%1”…").arg(card.presetName)
            available: card.actionsAvailable
            onClicked: card.actionRequested("delete")
        }
        CustomizeIconButton {
            objectName: "customizePresetRestore_" + card.presetId
            visible: card.modified
            implicitWidth: 32
            implicitHeight: 32
            iconName: "edit-undo"
            toolTip: qsTr("Restore the original “%1”…").arg(card.presetName)
            available: card.actionsAvailable
            onClicked: card.actionRequested("restore")
        }
        CustomizeIconButton {
            objectName: "customizePresetSaveAs_" + card.presetId
            visible: card.modified
            implicitWidth: 32
            implicitHeight: 32
            iconName: "document-save-as"
            toolTip: qsTr("Save “%1” as a new preset…").arg(card.presetName)
            available: card.actionsAvailable
            onClicked: card.actionRequested("saveAs")
        }
    }
}
