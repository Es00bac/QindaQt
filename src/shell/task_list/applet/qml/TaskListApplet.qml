// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Compiled task-list panel strip. The controller is the composed shell facade
// injected above QML as `access`; this file owns no state, no transport, and
// no window authority — every gesture re-enters the controller, which applies
// capability, generation, and pending fences before any dispatch.
//
// AGENT-CONTRACT: presentation consumes semantic QST-1 roles through the
// QindaQt.Tokens singleton and the compiled QindaQt.Controls primitives
// (module-boundaries rule: first-party presentation imports QindaQt.Controls
// explicitly). This file must not grow palette literals, theme-id knowledge,
// or its own fallback colors — Controls/Tokens are the only theme authority.
Item {
    id: root

    required property var access
    property bool vertical: false
    // The panel/profile owner selects dock mode. Keeping it opt-in preserves
    // the compact taskbar contract for every existing host.
    property bool dockMode: false
    // Instance-level worn Luna dressing (ADR-0124), set through this applet's
    // own profile settings; every other host renders token visuals.
    property bool luna: false
    // Manifest `grouping`: "never" gives every window, container members
    // included, its own button (controller windowRows); other values keep one
    // button per container. Dock strips keep container rows: they reorder tasks.
    property string grouping: "when-crowded"
    readonly property bool ungrouped: grouping === "never" && !dockMode
    readonly property var taskRows: access === null ? []
        : ungrouped ? access.windowRows : access.entryRows
    readonly property int presentedOverflowCount: access === null ? 0
        : ungrouped ? access.windowOverflowCount : access.overflowCount
    property int dockTileSize: 60
    property bool dockHasLauncherGroup: false
    property bool reducedMotion: false
    // Host quick setting (panels.configuration): the macOS-style pointer
    // magnification of dock tiles. reducedMotion always wins over it.
    property bool dockZoomEnabled: true
    readonly property int resolvedDockTileSize: Math.max(56, Math.min(64, dockTileSize))
    // AGENT-GUARD: magnification transforms tile visuals only — delegate
    // sizes, layout bounds, and hit targets never change, so GridLayout and
    // the zone viewport geometry stay exact. The tile reserves its full
    // envelope before the icon swells (panel hit-targets contract).
    readonly property bool dockZoomActive:
        dockMode && dockZoomEnabled && !reducedMotion && stripVisible
    // Pointer x in strip coordinates while the pointer is over the strip; -1
    // when zoom is inactive so every tile returns to rest scale.
    property real dockPointerX: -1
    readonly property real dockZoomPeak: 1.5

    // Gaussian proximity falloff: the tile under the pointer peaks, and about
    // two neighbors on each side participate. centerX is the tile center in
    // strip coordinates.
    function dockZoomFor(centerX) {
        if (!dockZoomActive || dockPointerX < 0) {
            return 1.0
        }
        const sigma = resolvedDockTileSize * 1.5
        const distance = centerX - dockPointerX
        return 1.0 + (dockZoomPeak - 1.0)
            * Math.exp(-0.5 * (distance / sigma) * (distance / sigma))
    }

    // Drag-and-drop manual reorder (dock mode). One passive DragHandler on
    // the strip arbitrates: presses stay with the tiles, and only a drag that
    // crosses the drag threshold takes the exclusive grab — a click never
    // activates, and a drag never re-enters the compositor (reorderTask is a
    // presentation preference with its own displayed-revision fence).
    readonly property bool dockDragArmed: dockMode && stripVisible && vertical === false
    property int dockDragFrom: -1
    property int dockDragTo: -1
    property string dockDragTaskId: ""
    readonly property bool dockDragActive: dockDragFrom >= 0
    readonly property real dockSlotExtent: resolvedDockTileSize + Tokens.space["1"]
    readonly property real dockDragFollowX: dockDragActive
        ? Math.max(-2.5 * dockSlotExtent,
                   Math.min(2.5 * dockSlotExtent, dockDrag.translation.x))
        : 0

    DragHandler {
        id: dockDrag
        enabled: root.dockDragArmed
        acceptedButtons: Qt.LeftButton
        yAxis.enabled: false
        onActiveChanged: {
            if (active) {
                root.dockDragBegin(Math.floor(
                    (centroid.pressPosition.x + root.dockSlotExtent / 2)
                    / root.dockSlotExtent))
            } else {
                root.dockDragEnd()
            }
        }
        onTranslationChanged: root.dockDragUpdate(centroid.position.x)
    }

    function dockDragBegin(fromIndex) {
        const rows = access !== null ? access.entryRows : []
        if (fromIndex < 0 || fromIndex >= rows.length) {
            dockDragFrom = -1
            return
        }
        dockDragFrom = fromIndex
        dockDragTo = fromIndex
        dockDragTaskId = String(rows[fromIndex].taskId)
    }

    function dockDragUpdate(pointerX) {
        const rows = access !== null ? access.entryRows : []
        dockDragTo = Math.max(0, Math.min(
            rows.length, Math.round(pointerX / dockSlotExtent)))
    }

    function dockDragEnd() {
        const from = dockDragFrom
        const target = dockDragTo
        const taskId = dockDragTaskId
        dockDragFrom = -1
        dockDragTo = -1
        dockDragTaskId = ""
        if (from < 0 || access === null) {
            return
        }
        // Dropped back into its own slot or the gap right behind it: the
        // order did not change, so nothing is committed or announced.
        if (target === from || target === from + 1) {
            return
        }
        const rows = access.entryRows
        if (from >= rows.length) {
            return
        }
        const beforeId = target >= rows.length ? "" : String(rows[target].taskId)
        access.reorderTask(taskId, beforeId, rows[from].generationRevision)
    }

    // Hover preview state (ADR-0119). At most one tile preview is visible;
    // every hover change supersedes the previous request, and the 500 ms
    // refresh keeps a static hover roughly current without flooding the
    // compositor with captures.
    property int previewIndex: -1
    property string previewTaskId: ""
    property int previewToken: 0
    property string previewTitle: ""
    readonly property bool previewVisible: previewIndex >= 0

    Timer {
        id: previewRefresh
        interval: 500
        repeat: true
        running: root.previewVisible
        onTriggered: root.requestPreviewNow()
    }

    function previewHover(index, hovered) {
        if (!hovered) {
            if (previewIndex === index) {
                closePreview()
            }
            return
        }
        if (access === null || !access.previewsEnabled) {
            return
        }
        const rows = root.taskRows
        if (index < 0 || index >= rows.length) {
            return
        }
        previewIndex = index
        previewTaskId = String(rows[index].taskId)
        previewTitle = String(rows[index].accessibleName)
        previewToken = 0
        previewPopup.open()
        requestPreviewNow()
        previewRefresh.restart()
    }

    function requestPreviewNow() {
        if (previewIndex < 0 || access === null) {
            return
        }
        const rows = root.taskRows
        if (previewIndex >= rows.length) {
            closePreview()
            return
        }
        const row = rows[previewIndex]
        access.requestTaskPreview(String(row.taskId), row.generationRevision,
                                  320, 200)
    }

    function closePreview() {
        previewPopup.close()
        previewIndex = -1
        previewTaskId = ""
        previewToken = 0
        previewTitle = ""
        previewRefresh.stop()
        if (access !== null) {
            access.cancelTaskPreview()
        }
    }

    Connections {
        // access is optional (the applet renders disconnected in previews and
        // early startup), so unknown-signal resolution must not be fatal.
        target: root.access
        ignoreUnknownSignals: true
        function onPreviewArrived(taskId, revision, imageToken) {
            if (root.previewVisible && taskId === root.previewTaskId) {
                root.previewToken = imageToken
            }
        }
    }

    // AGENT-GUARD: the popup opens and closes imperatively from the hover
    // state functions. Binding `visible` to the state flag re-entered the
    // property chain through the popup's own layout evaluation (binding-loop
    // warnings are fatal under QT_FATAL_WARNINGS in the qualification rows).
    TaskListPreviewPopup {
        id: previewPopup
        objectName: "taskListPreviewPopup"
        titleText: root.previewTitle
        imageToken: root.previewToken
        parent: root
        x: {
            const delegate = entryRepeater.itemAt(root.previewIndex)
            if (delegate === null) return 0
            return Math.max(0, delegate.x + delegate.width / 2 - width / 2)
        }
        // A bottom dock opens the card above the tile; a top taskbar opens
        // it below the row. The popup window is never clipped either way.
        y: root.dockMode ? -height - Tokens.space["2"]
                         : root.height + Tokens.space["2"]
    }

    // Keyboard parity for the drag gesture: the tile context menu's
    // "Move left/right" actions call this with the menu's own tile index.
    function moveTask(index, direction) {
        if (access === null || direction === 0) {
            return
        }
        const rows = access.entryRows
        if (index < 0 || index >= rows.length) {
            return
        }
        if (direction < 0 && index === 0) {
            return
        }
        if (direction > 0 && index >= rows.length - 1) {
            return
        }
        const beforeId = direction < 0
            ? String(rows[index - 1].taskId)
            : (index + 2 < rows.length ? String(rows[index + 2].taskId) : "")
        access.reorderTask(String(rows[index].taskId), beforeId,
                           rows[index].generationRevision)
        const focused = entryRepeater.itemAt(index + direction)
        if (focused !== null) {
            focused.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    readonly property string phase: access !== null ? access.phaseText : "unavailable"
    readonly property bool stripVisible: access !== null && access.entryCount > 0
    readonly property bool dockEmpty: dockMode && phase === "empty"

    // Passive hover: never consumes events; only tracks the strip-local
    // pointer x that drives the magnification falloff.
    HoverHandler {
        id: dockZoomHover
        enabled: root.dockZoomActive
        onPointChanged:
            root.dockPointerX = hovered ? point.position.x : -1
        onHoveredChanged:
            root.dockPointerX = hovered ? point.position.x : -1
    }

    objectName: "taskListApplet"
    visible: !dockEmpty
    implicitWidth: dockEmpty ? 0 : dockMode
        ? (vertical ? resolvedDockTileSize : strip.implicitWidth)
        : (vertical ? 44 : strip.implicitWidth)
    implicitHeight: dockEmpty ? 0 : dockMode
        ? (vertical ? strip.implicitHeight : resolvedDockTileSize)
        : (vertical ? strip.implicitHeight : 32)

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Task list")
    Accessible.description: {
        if (access === null)
            return qsTr("Task list controls are not connected")
        if (phase === "loading")
            return qsTr("The task list is loading")
        if (phase === "empty")
            return qsTr("No windows are visible in this scope")
        if (phase === "degraded")
            return qsTr("The task list source is limited; actions are paused")
        if (phase === "unavailable")
            return qsTr("The task list is unavailable")
        return qsTr("%1 windows").arg(access.totalEntryCount)
    }

    GridLayout {
        id: strip
        anchors.fill: parent
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rows: root.vertical ? -1 : 1
        columns: root.vertical ? 1 : -1
        rowSpacing: Tokens.space["1"]
        columnSpacing: Tokens.space["1"]

        C.Label {
            id: loadingLabel
            objectName: "taskListLoadingLabel"
            visible: false
            text: qsTr("Loading…")
            muted: true
        }

        C.Label {
            id: unavailableLabel
            objectName: "taskListUnavailableLabel"
            visible: false
            text: qsTr("Task list unavailable")
            muted: true
            Accessible.name: root.access !== null
                ? qsTr("Task list unavailable: %1").arg(root.access.phaseReasonText)
                : qsTr("Task list unavailable")
        }

        C.Label {
            id: emptyLabel
            objectName: "taskListEmptyLabel"
            visible: false
            text: qsTr("No windows")
            muted: true
        }

        ShellIcons.Icon {
            id: phaseIcon
            objectName: "taskListPhaseIcon"
            visible: !root.dockEmpty && !root.stripVisible && root.phase !== "degraded"
            name: root.phase === "loading" ? "view-refresh-symbolic"
                                             : "preferences-system-windows"
            size: 20
            color: Tokens.fg.muted
            symbolic: true
            fallbackText: qsTr("Task list")
            Accessible.ignored: true
        }

        Rectangle {
            id: dockGroupSeparator
            objectName: "taskListDockGroupSeparator"
            visible: root.dockMode && root.dockHasLauncherGroup && root.stripVisible
            implicitWidth: root.vertical ? root.resolvedDockTileSize : 1
            implicitHeight: root.vertical ? 1 : root.resolvedDockTileSize
            color: Tokens.outline.divider
            Accessible.ignored: true
        }

        Repeater {
            id: entryRepeater
            model: root.stripVisible ? root.taskRows : []

            delegate: TaskListEntryButton {
                required property var modelData
                required property int index

                entry: modelData
                access: root.access
                vertical: root.vertical
                dockMode: root.dockMode
                dockTileSize: root.resolvedDockTileSize
                reducedMotion: root.reducedMotion
                luna: root.luna
                // The delegate's x is strip-local because the strip fills the
                // applet; the falloff binding re-evaluates on every pointer
                // move and on layout changes.
                dockZoomScale: root.dockZoomFor(x + width / 2)
                // Drag-reorder visuals: the dragged tile follows the pointer,
                // and the tiles between the source and the insertion gap shift
                // one slot so the drop position reads before the drop. Both
                // are transforms — layout bounds never move.
                readonly property bool dragHeld:
                    root.dockDragActive && index === root.dockDragFrom
                readonly property real dragShiftX: {
                    if (!root.dockDragActive || index === root.dockDragFrom) {
                        return 0
                    }
                    const from = root.dockDragFrom
                    const target = root.dockDragTo
                    if (target > from + 1) {
                        return (index > from && index < target)
                            ? -root.dockSlotExtent : 0
                    }
                    if (target < from) {
                        return (index >= target && index < from)
                            ? root.dockSlotExtent : 0
                    }
                    return 0
                }
                dragFollowX: dragHeld ? root.dockDragFollowX : 0
                z: dragHeld ? 2 : 0
                stripMove: root.moveTask
                stripPreviewHover: root.previewHover
                stripClosePreview: root.closePreview

                KeyNavigation.left: root.vertical
                    ? null : entryRepeater.itemAt(index - 1)
                KeyNavigation.right: root.vertical
                    ? null : entryRepeater.itemAt(index + 1)
                KeyNavigation.up: root.vertical
                    ? entryRepeater.itemAt(index - 1) : null
                KeyNavigation.down: root.vertical
                    ? entryRepeater.itemAt(index + 1) : null
            }
        }

        // Overflow truth: the controller caps presented rows and reports the
        // exact hidden count; the strip never silently drops entries.
        C.Label {
            id: overflowIndicator
            objectName: "taskListOverflowIndicator"
            visible: root.presentedOverflowCount > 0
            text: visible ? qsTr("+%1 more").arg(root.presentedOverflowCount) : ""
            muted: true
            Accessible.name: visible
                ? qsTr("%1 further windows are not shown")
                      .arg(root.presentedOverflowCount)
                : ""
        }

        Item {
            id: degradedBadge
            objectName: "taskListDegradedBadge"
            visible: root.phase === "degraded"
            implicitWidth: 18
            implicitHeight: 18
            Accessible.role: Accessible.StaticText
            Accessible.name: root.access !== null
                ? qsTr("Task list source is limited: %1")
                      .arg(root.access.phaseReasonText)
                : ""
            Accessible.description: qsTr(
                "Window buttons remain visible, but actions are paused")

            ShellIcons.Icon {
                anchors.fill: parent
                name: "dialog-warning"
                size: 18
                color: Tokens.fg.default
                symbolic: true
                fallbackText: qsTr("Warning")
                Accessible.ignored: true
            }
        }
    }

    // Intent feedback (refusals, failures, uncertainty) surfaces as a
    // dismissable non-modal notice with an alert role.
    T.Popup {
        id: feedbackPopup
        objectName: "taskListFeedbackPopup"
        visible: root.access !== null && root.access.feedbackPresent
        modal: false
        focus: visible
        closePolicy: T.Popup.CloseOnEscape
        padding: Tokens.space["2"]
        parent: root

        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)

        background: Rectangle {
            radius: Tokens.radius.m
            color: Tokens.bg.raised
            border.color: Tokens.outline.strong
        }

        contentItem: RowLayout {
            spacing: Tokens.space["2"]

            C.Label {
                id: feedbackText
                objectName: "taskListFeedbackText"
                Layout.maximumWidth: 320
                text: root.access !== null ? root.access.feedback : ""
                Accessible.role: Accessible.AlertMessage
            }

            C.Button {
                id: feedbackDismiss
                objectName: "taskListFeedbackDismiss"
                text: qsTr("Dismiss")
                emphasized: false
                accessibleDescription: qsTr("Dismiss task list notice")
                onClicked: {
                    if (root.access !== null)
                        root.access.clearFeedback()
                }
                Accessible.onPressAction: {
                    if (root.access !== null)
                        root.access.clearFeedback()
                }
            }
        }
    }
}
