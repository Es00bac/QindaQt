// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import "DisplayArrangementGeometry.js" as Geometry

// Section for relative multi-monitor positioning and the primary display role:
// a scaled diagram to drag, quick placement for the common cases, and the
// exact coordinates as an optional precision control. Every change is a draft
// through the public facade until the user chooses Apply.
ColumnLayout {
    id: root

    required property var displaySettings
    required property bool editorBusy

    readonly property bool isPrimary: root.displaySettings.selectedOutput.primary ?? false
    readonly property bool outputEnabled: root.displaySettings.selectedOutput.enabled ?? false
    readonly property int posX: root.displaySettings.selectedOutput.positionX ?? 0
    readonly property int posY: root.displaySettings.selectedOutput.positionY ?? 0
    readonly property bool arrangeEnabled: root.displaySettings.canEdit && !root.editorBusy
    readonly property string selectedLabel: String(root.displaySettings.selectedOutput.label
                                                   ?? root.displaySettings.selectedOutput.connectorName
                                                   ?? "")
    // Stable ids of connected-but-disabled outputs; only reassigned when the
    // set changes so their tiles survive ordinary draft edits.
    property var inactiveIds: []

    // Last enabled geometry seen from the facade, used to notice a display
    // whose logical size changed in place (scale, mode, rotation).
    property var knownRects: []

    spacing: Tokens.space["3"]

    function selectedRect() {
        return Geometry.findRect(arrangementCanvas.rects,
                                 root.displaySettings.selectedOutputId)
    }

    function readoutText() {
        if (arrangementCanvas.dragging && arrangementCanvas.dragPreview !== null) {
            const preview = arrangementCanvas.dragPreview
            return qsTr("Moving %1 to %2, %3").arg(root.selectedLabel)
                    .arg(preview.x).arg(preview.y)
        }
        const rect = root.selectedRect()
        if (rect !== null) {
            return qsTr("%1 %2 at %3, %4 — %5 × %6 logical")
                    .arg(arrangementCanvas.ordinalFor(rect.stableId))
                    .arg(root.selectedLabel).arg(rect.x).arg(rect.y)
                    .arg(rect.width).arg(rect.height)
        }
        if (root.selectedLabel.length > 0 && !root.outputEnabled) {
            return qsTr("%1 is off. Turn on Enable display to place it.").arg(root.selectedLabel)
        }
        return ""
    }

    // Moves computed from the latest outputsChanged, applied on the next
    // event-loop turn so the model never mutates inside its own emission.
    property var pendingMoves: []
    property string pendingBaseline: ""

    // AGENT-GUARD: Detection is synchronous on every outputsChanged so each
    // draft edit is compared with the geometry immediately before it; a
    // deferred comparison would fold two quick edits into one and misread a
    // moved neighbour as a resize. The draft is only touched when the user's
    // own dirty draft changed one enabled display's size in place; snapshot
    // resets and reverts (draftDirty false) just refresh the cache so applied
    // truth is never "repaired" behind the user's back.
    function noticeOutputsChanged() {
        const next = Geometry.enabledRects(root.displaySettings.outputs)
        const previous = root.knownRects
        root.knownRects = next
        root.refreshInactiveIds()
        if (!root.displaySettings.draftDirty || !root.arrangeEnabled) {
            return
        }
        const resizedId = Geometry.resizedInPlace(previous, next)
        if (resizedId === null) {
            return
        }
        const moves = Geometry.reattachAfterResize(previous, next, resizedId)
        if (moves.length === 0) {
            return
        }
        root.pendingMoves = moves
        root.pendingBaseline = JSON.stringify(next)
        Qt.callLater(root.applyPendingMoves)
    }

    function applyPendingMoves() {
        const moves = root.pendingMoves
        const baseline = root.pendingBaseline
        root.pendingMoves = []
        root.pendingBaseline = ""
        if (!root.arrangeEnabled
                || JSON.stringify(Geometry.enabledRects(root.displaySettings.outputs)) !== baseline) {
            return
        }
        for (let index = 0; index < moves.length; ++index) {
            root.displaySettings.setOutputPosition(moves[index].stableId,
                                                   moves[index].x, moves[index].y)
        }
    }

    function refreshInactiveIds() {
        const list = root.displaySettings.outputs ?? []
        const next = []
        for (let index = 0; index < list.length; ++index) {
            if (!list[index].enabled) {
                next.push(String(list[index].stableId))
            }
        }
        if (next.join("\n") !== root.inactiveIds.join("\n")) {
            root.inactiveIds = next
        }
    }

    Component.onCompleted: {
        root.knownRects = Geometry.enabledRects(root.displaySettings.outputs)
        root.refreshInactiveIds()
    }

    Connections {
        target: root.displaySettings
        function onOutputsChanged() {
            root.noticeOutputsChanged()
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Arrangement")
        description: qsTr("Drag displays to match how they sit on your desk. Numbers match the display cards above; sizes reflect resolution, scale, and rotation.")
    }

    DisplayArrangementCanvas {
        id: arrangementCanvas
        objectName: "displayArrangementCanvas"
        Layout.fillWidth: true
        Layout.preferredHeight: implicitHeight
        outputs: root.displaySettings.outputs
        selectedOutputId: root.displaySettings.selectedOutputId
        canEdit: root.arrangeEnabled
        onSelectRequested: stableId => root.displaySettings.setSelectedOutputId(stableId)
        onMoveRequested: (stableId, x, y) => root.displaySettings.setOutputPosition(stableId, x, y)
    }

    Label {
        objectName: "displayArrangementReadout"
        Layout.fillWidth: true
        text: root.readoutText()
        visible: text.length > 0
        font.pointSize: Tokens.type.caption
        muted: true
        textFormat: Text.PlainText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    RowLayout {
        objectName: "displayArrangementInactiveRow"
        Layout.fillWidth: true
        visible: root.inactiveIds.length > 0
        spacing: Tokens.space["2"]

        Label {
            text: qsTr("Not in use:")
            muted: true
            font.pointSize: Tokens.type.caption
        }

        Flow {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Repeater {
                model: root.inactiveIds

                delegate: DisplayArrangementTile {
                    id: inactiveTile
                    required property string modelData

                    objectName: "displayArrangementInactiveTile_" + inactiveTile.modelData
                    width: 96
                    height: 48
                    output: arrangementCanvas.outputFor(inactiveTile.modelData)
                    ordinal: arrangementCanvas.ordinalFor(inactiveTile.modelData)
                    selected: root.displaySettings.selectedOutputId === inactiveTile.modelData
                    draggable: false
                    inactive: true
                    onSelectRequested: root.displaySettings.setSelectedOutputId(
                                           inactiveTile.modelData)
                }
            }
        }
    }

    DisplayPlacementControls {
        id: placementControls
        objectName: "displayPlacementControls"
        Layout.fillWidth: true
        outputs: root.displaySettings.outputs
        selectedOutputId: root.displaySettings.selectedOutputId
        selectedEnabled: root.outputEnabled
        canEdit: root.arrangeEnabled
        onPlaceRequested: (side, referenceId) => arrangementCanvas.placeBeside(
                              root.displaySettings.selectedOutputId, referenceId, side)
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Primary display")
        description: qsTr("Designate this monitor as the primary workspace display")
        editor: primarySwitch

        Switch {
            id: primarySwitch
            objectName: "displayPrimarySwitch"
            text: checked ? qsTr("Primary Display") : qsTr("Secondary Display")
            checked: root.isPrimary
            enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled && !root.isPrimary
            onToggled: {
                if (checked && root.displaySettings.selectedOutputId) {
                    root.displaySettings.setOutputPrimary(root.displaySettings.selectedOutputId)
                }
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Exact position (X, Y)")
        description: qsTr("Optional precision control in logical pixels; negative values place a display left of or above the origin. Changes apply after you choose Apply.")
        editor: positionRow

        RowLayout {
            id: positionRow
            spacing: Tokens.space["2"]

            DisplayCoordinateField {
                id: posXField
                objectName: "displayPosXField"
                implicitWidth: 100
                outputId: root.displaySettings.selectedOutputId
                authoritativeValue: root.posX
                coordinateName: qsTr("Position X coordinate")
                enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                onValidCommitRequested: (originOutputId, value) => {
                    if (originOutputId === root.displaySettings.selectedOutputId) {
                        root.displaySettings.setOutputPosition(
                            originOutputId, value, root.posY)
                    }
                }
            }

            Label {
                text: "×"
                color: Tokens.fg.muted
            }

            DisplayCoordinateField {
                id: posYField
                objectName: "displayPosYField"
                implicitWidth: 100
                outputId: root.displaySettings.selectedOutputId
                authoritativeValue: root.posY
                coordinateName: qsTr("Position Y coordinate")
                enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                onValidCommitRequested: (originOutputId, value) => {
                    if (originOutputId === root.displaySettings.selectedOutputId) {
                        root.displaySettings.setOutputPosition(
                            originOutputId, root.posX, value)
                    }
                }
            }
        }
    }
}
