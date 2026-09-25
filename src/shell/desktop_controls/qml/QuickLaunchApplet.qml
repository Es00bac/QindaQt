// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// The dock (ADR-0265; "quick launch" on a taskbar): the launcher-owned dock
// value as tiles — pinned applications, folders, files, groups, and Trash.
// Activation, grouping, reordering, and removal all re-enter the dock facade;
// this file only decides which call a click, drop, drag, or menu makes.
//
// Dragging a tile along the strip moves it (live gap), onto an application
// or group groups it, and off the strip across the panel removes it. Things
// dropped from elsewhere (launcher rows, .desktop files, files, folders, a
// group member dragged out of its popup) are added at the pointer. The menu
// repeats every gesture for keyboard users.
Item {
    id: root

    required property var access
    property bool vertical: false
    // Panel composition elects this compact dock presentation. No pin is
    // synthesized here: rows remain the launcher facade's persisted dock.
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    // Host quick setting: dock magnification (same contract as the task
    // strip's). reducedMotion always wins over it.
    property bool dockZoomEnabled: true
    // Values below the panel's 24px transient-fit contract are malformed host
    // input and retain the historical compact fail-safe rather than creating
    // an unusable button.
    readonly property int resolvedDockTileSize:
        dockTileSize < 24 ? 56 : Math.min(64, dockTileSize)

    // Worn Luna dressing (ADR-0124): instance-level opt-in from the profile.
    property bool luna: false
    // ADR-0268: which of the dock's items this instance shows (profile
    // setting `items`). "file-manager" and "trash" are the Mac-style dock's
    // permanent ends, one fixed tile each that is never stored, moved, or
    // removed; "others" is every stored item but those two, for the dock that
    // holds both ends; anything else is the whole dock.
    property string items: "all"
    readonly property bool fixedEnd: items === "file-manager" || items === "trash"
    readonly property bool ready: access !== null && Tokens.ready
    readonly property string fileManagerEntry:
        ready ? String(access.fileManagerRow?.entryId ?? "") : ""
    readonly property var rows: !ready ? []
        : items === "file-manager"
            ? (fileManagerEntry !== "" ? [access.fileManagerRow] : [])
        : items === "trash" ? (access.trashRow ? [access.trashRow] : [])
        : items === "others"
            ? access.rows.filter(row => String(row.kind) !== "trash"
                                 && String(row.entryId) !== fileManagerEntry)
        : access.rows
    readonly property bool showRows: ready && rows.length > 0
    readonly property int iconExtent: dockMode
        ? Math.min(40, Math.max(16, resolvedDockTileSize - 8))
                                               : Math.max(0, Math.min(20, (vertical ? width : height) - Tokens.space["2"]))
    readonly property int tileExtent: dockMode ? resolvedDockTileSize
                                               : iconExtent + Tokens.space["2"] * 2
    readonly property real slotExtent: tileExtent + Tokens.space["1"]
    // AGENT-GUARD: magnification transforms tile visuals only — delegate
    // sizes, layout bounds, and hit targets never change. Tiles are pinned to
    // Layout preferred sizes, so the swell cannot move the strip.
    readonly property bool dockZoomActive:
        dockMode && dockZoomEnabled && !reducedMotion && showRows
    // Pointer x in strip coordinates while hovered; -1 when zoom is inactive.
    property real dockPointerX: -1
    readonly property real dockZoomPeak: 1.5
    // The tile icon's upward growth envelope over this strip's own tile: the
    // bottom-anchored swell at dockZoomPeak plus the 3px hover lift. It must
    // equal PanelAppletRow.dockOverscanFor over the same effective tile — the
    // panel viewport exposes exactly this much headroom above the shelf and
    // the input/blur masks reserve the same band, so tracking or painting
    // beyond it would leave the masked surface, and less would collapse the
    // zoom under a pointer riding the magnified bump. Dock-only geometry;
    // the tracking surface below stays unpickable outside dock zoom.
    readonly property real dockZoomEnvelope:
        Math.max(0, (dockMode ? iconExtent
                              : Math.min(40, Math.max(16, resolvedDockTileSize - 8)))
                   - resolvedDockTileSize / 2) + 3

    // AGENT-NOTE: an empty dock is a zero-size chip yet must take its first
    // drop, so its drop area is a square reaching one tile each way from
    // that point, over neighbours that accept no drops (the zone viewport
    // still clips it to the panel).
    readonly property real emptyDropReach: tileExtent

    function dockZoomFor(centerX) {
        if (!dockZoomActive || dockPointerX < 0) {
            return 1.0
        }
        const sigma = resolvedDockTileSize * 1.5
        const distance = centerX - dockPointerX
        return 1.0 + (dockZoomPeak - 1.0)
            * Math.exp(-0.5 * (distance / sigma) * (distance / sigma))
    }

    HoverHandler {
        id: dockZoomHover
        enabled: root.dockZoomActive
        onPointChanged:
            root.dockPointerX = hovered ? point.position.x : -1
        onHoveredChanged:
            root.dockPointerX = hovered ? point.position.x : -1
    }

    // AGENT-GUARD: the tracking surface is the band strictly above the tiles
    // and is pickable only while magnification is active, so it never becomes
    // the hover target over a tile's own hit area and never shades panel
    // content on non-dock hosts. It keeps the falloff following a pointer
    // that rides the magnified bump above the shelf instead of collapsing
    // the zoom at the shelf line.
    Item {
        id: dockZoomSurface
        objectName: "quickLaunchDockZoomSurface"
        x: 0
        y: -root.dockZoomEnvelope
        width: parent.width
        height: root.dockZoomEnvelope
        visible: root.dockZoomActive

        HoverHandler {
            enabled: root.dockZoomActive
            onPointChanged:
                root.dockPointerX = hovered ? point.position.x : -1
            onHoveredChanged:
                root.dockPointerX = hovered ? point.position.x : -1
        }
    }

    objectName: "quickLaunchApplet"
    // An empty dock stays present (zero width) so it can accept a drop.
    visible: !dockMode || showRows || ready
    implicitWidth: showRows ? strip.implicitWidth
                            : (dockMode ? 0 : placeholder.implicitWidth + Tokens.space["2"])
    implicitHeight: showRows ? strip.implicitHeight : (dockMode ? 0 : 28)

    Accessible.role: Accessible.Grouping
    Accessible.name: dockMode ? qsTr("Dock") : qsTr("Quick launch")
    Accessible.description: !ready ? qsTr("The dock is not connected")
                            : rows.length === 0 ? qsTr("Nothing is kept in the dock")
                            : qsTr("%1 items").arg(rows.length)

    // AGENT-GUARD: a dock change rebuilds every tile (new rows array), which
    // destroys the focused one. A keyboard move asks for focus on the moved
    // tile; the request outlives the local preview and the Settings1 confirm
    // rebuilds, then expires so a later rebuild never pulls focus back.
    property int keyboardFocusPosition: -1
    Timer {
        id: keyboardFocusExpiry
        interval: 3000
        onTriggered: root.keyboardFocusPosition = -1
    }

    function focusIndex(index) {
        if (index < 0 || index >= repeater.count)
            return
        const item = repeater.itemAt(index)
        if (item !== null)
            item.forceActiveFocus(Qt.TabFocusReason)
    }

    function activateRow(position, item) {
        const row = rows[position]
        if (row === undefined || !ready)
            return
        const kind = String(row.kind)
        if (kind === "group" || kind === "folder")
            stackPopup.show(row, item)
        else if (kind === "application")
            access.activate(String(row.entryId))
        else if (Boolean(row.fixed))
            access.openTrash()
        else
            access.activateItem(Number(row.index))
    }

    function openMenu(position, item) {
        itemMenu.row = rows[position] ?? ({})
        itemMenu.visualIndex = position
        itemMenu.anchorTile = item
        itemMenu.popup(item)
    }

    function tileAt(position) {
        return repeater.itemAt(position)
    }

    DockGestures {
        id: gestures
        objectName: "quickLaunchGestures"
        access: root.access
        ready: root.ready
        rows: root.rows
        vertical: root.vertical
        slotExtent: root.slotExtent
        tileExtent: root.tileExtent
        crossExtent: root.vertical ? root.width : root.height
        onFocusRequested: (position) => {
            root.keyboardFocusPosition = position
            keyboardFocusExpiry.restart()
            Qt.callLater(root.focusIndex, position)
        }
    }

    ShellIcons.Icon {
        id: placeholder
        objectName: "quickLaunchPlaceholder"
        visible: !root.showRows && !root.dockMode
        anchors.centerIn: parent
        name: "applications-other"
        size: 18
        color: root.luna ? "white" : Tokens.fg.muted
        symbolic: true
        fallbackText: qsTr("Quick launch")
        Accessible.ignored: true
    }

    // One passive DragHandler on the strip arbitrates tile drags: presses stay
    // with the tiles, and only a drag past the threshold takes the grab, so a
    // click never moves anything. The pointer is tracked outside the strip
    // during the grab, which is how dragging across the panel removes a tile.
    DragHandler {
        id: tileDrag
        target: null
        enabled: root.showRows && Boolean(root.access.editable) && !root.fixedEnd
        acceptedButtons: Qt.LeftButton
        onActiveChanged: {
            if (active)
                gestures.dragBegin(centroid.pressPosition)
            else
                gestures.dragEnd()
        }
        onTranslationChanged: gestures.dragUpdate(centroid.position, centroid.pressPosition)
    }

    DropArea {
        id: dropArea
        objectName: "quickLaunchDropArea"
        x: root.showRows ? 0 : root.width / 2 - root.emptyDropReach
        y: root.showRows ? 0 : root.height / 2 - root.emptyDropReach
        width: root.showRows ? root.width : root.emptyDropReach * 2
        height: root.showRows ? root.height : root.emptyDropReach * 2
        enabled: root.ready && !root.fixedEnd
        onEntered: (drag) => {
            const kind = gestures.dropKind(drag)
            drag.accepted = kind !== "" && Boolean(root.access.editable)
            if (drag.accepted)
                gestures.dropHover(Qt.point(drag.x + dropArea.x, drag.y + dropArea.y), kind)
        }
        onPositionChanged: (drag) =>
            gestures.dropHover(Qt.point(drag.x + dropArea.x, drag.y + dropArea.y),
                               gestures.dropKind(drag))
        onExited: gestures.dragReset()
        onDropped: (drop) => {
            if (gestures.dropCommit(drop))
                drop.acceptProposedAction()
            else
                drop.accepted = false
        }
    }

    GridLayout {
        id: strip
        anchors.fill: parent
        visible: root.showRows
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rows: root.vertical ? -1 : 1
        columns: root.vertical ? 1 : -1
        rowSpacing: Tokens.space["1"]
        columnSpacing: Tokens.space["1"]

        Repeater {
            id: repeater
            model: root.rows

            DockItemTile {
                id: tileDelegate

                required property var modelData
                required property int index

                row: modelData
                visualIndex: index
                vertical: root.vertical
                dockMode: root.dockMode
                tileExtent: root.tileExtent
                iconExtent: root.iconExtent
                reducedMotion: root.reducedMotion
                luna: root.luna
                launchEnabled: root.ready && Boolean(root.access.launchGranted)
                zoomScale: root.dockZoomFor(x + width / 2)
                mainShift: gestures.shiftFor(index)
                dragHeld: index === gestures.dragFrom
                mergeTarget: index === gestures.hoverTarget
                removing: gestures.dragRemoving && index === gestures.dragFrom
                onActivated: root.activateRow(index, tileDelegate)
                onContextRequested: root.openMenu(index, tileDelegate)
                Keys.onLeftPressed: if (!root.vertical) root.focusIndex(index - 1)
                Keys.onRightPressed: if (!root.vertical) root.focusIndex(index + 1)
                Keys.onUpPressed: if (root.vertical) root.focusIndex(index - 1)
                Keys.onDownPressed: if (root.vertical) root.focusIndex(index + 1)
                Component.onCompleted: if (index === root.keyboardFocusPosition) forceActiveFocus(Qt.OtherFocusReason)
            }
        }
    }

    // "Drag here to remove": said in words while a tile hovers off the strip.
    C.Label {
        objectName: "quickLaunchRemoveHint"
        visible: gestures.dragRemoving
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        text: qsTr("Release to remove from the Dock")
        Accessible.role: Accessible.AlertMessage
    }

    DockItemMenu {
        id: itemMenu
        vertical: root.vertical
        visualCount: root.rows.length
        // A dock split into slices keeps its Trash at its end.
        trashInDock: root.ready && (Boolean(root.access.trashInDock) || root.items !== "all")
        onOpenRequested: root.activateRow(itemMenu.visualIndex, itemMenu.anchorTile)
        onOpenNewWindowRequested: root.access.openNewWindow(String(itemMenu.row.entryId))
        onOpenInFileManagerRequested: Boolean(itemMenu.row.fixed)
            ? root.access.openTrash() : root.access.openInFileManager(Number(itemMenu.row.index))
        onEmptyTrashRequested: promptPopup.ask("emptyTrash", Number(itemMenu.row.index), "",
                                               itemMenu.anchorTile)
        onNewGroupRequested: {
            const title = gestures.categoryTitle(String(itemMenu.row.categoryIdentity ?? ""))
            promptPopup.ask("newGroup", Number(itemMenu.row.index),
                            title.length > 0 ? title : qsTr("New Group"), itemMenu.anchorTile)
        }
        onRenameRequested: promptPopup.ask("rename", Number(itemMenu.row.index),
                                           String(itemMenu.row.displayText), itemMenu.anchorTile)
        onUngroupRequested: root.access.ungroup(Number(itemMenu.row.index))
        onMoveRequested: (direction) => gestures.moveRow(itemMenu.visualIndex, direction)
        onRemoveRequested: root.access.removeItem(Number(itemMenu.row.index))
        onShowTrashRequested: root.access.showTrash()
    }

    DockStackPopup {
        id: stackPopup
        access: root.access
        reducedMotion: root.reducedMotion
        vertical: root.vertical
        dockRows: root.rows
        tileAt: root.tileAt
    }

    DockPromptPopup {
        id: promptPopup
        vertical: root.vertical
        onConfirmed: (mode, targetIndex, text) => {
            if (!root.ready)
                return
            if (mode === "emptyTrash")
                root.access.emptyTrash()
            else if (mode === "newGroup")
                root.access.newGroup(targetIndex, text)
            else
                root.access.renameGroup(targetIndex, text)
        }
    }

    ControlPopupFrame {
        id: feedbackPopup
        objectName: "quickLaunchFeedbackPopup"
        // The group/folder popup shows the same line inline while it is open;
        // a permanent end leaves the shared line to its dock's other slices.
        visible: root.ready && Boolean(root.access.feedbackPresent) && !stackPopup.visible
                 && !root.fixedEnd
        heading: root.dockMode ? qsTr("Dock") : qsTr("Quick launch")
        feedback: root.ready ? String(root.access.feedback) : ""
        initialFocusItem: dismiss

        C.Button {
            id: dismiss
            objectName: "quickLaunchFeedbackDismiss"
            Layout.alignment: Qt.AlignRight
            text: qsTr("Dismiss")
            emphasized: false
            onClicked: root.access.clearFeedback()
        }
    }
}
