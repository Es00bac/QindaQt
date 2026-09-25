// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

Flickable {
    id: root

    required property var panel
    required property var theme
    required property string zone
    property bool liveApplets: false
    // Live customization facade and the hosting PanelContent (drop targets).
    property var liveCustomization: null
    property var editorHost: null
    property var notificationCenterAppletAccess: null
    property var audioAppletAccess: null
    property var bluetoothAppletAccess: null
    property var networkAppletAccess: null
    property var smartLightsAppletAccess: null
    property var voiceAppletAccess: null
    property var obsAppletAccess: null
    property var gatherOverviewAccess: null
    property var clipboardAppletAccess: null
    property var powerAppletAccess: null
    property var launcherAppletAccess: null
    property var globalMenuAppletAccess: null
    property var taskListAppletAccess: null
    property var statusNotifierAppletAccess: null
    property var desktopControlsAccess: null
    property bool vertical: false
    property bool dockMode: false
    property int dockTileSize: 60
    property real dockAvailableExtent: width
    property real dockSurfaceHeight: height
    property bool reducedMotion: false
    property bool dockZoomEnabled: true
    // Worn Luna taskbar (ADR-0124): PanelContent's panel-derived lunaMode,
    // forwarded to every chip of this zone. PanelAppletColumn inherits it.
    property bool lunaMode: false
    readonly property int lanes: Math.max(1, Number(panel.rows ?? 1))
    readonly property var zoneApplets: (panel.applets ?? []).filter(
        applet => appletZone(applet) === zone)
    readonly property int minimumFittedDockTileSize: 24
    // ADR-0265: in a dock zone that shows the pins, a running pinned
    // application is its pinned tile; the task strip leaves those tasks out
    // and the fit arithmetic below counts only the tiles still shown.
    readonly property var dockClaimedTaskIds: {
        if (!dockMode || !zoneApplets.some(applet => String(applet.plugin ?? "") === "quick-launch"))
            return []
        const quick = desktopControlsAccess !== null ? desktopControlsAccess.quickLaunch : null
        return quick !== null && quick !== undefined ? (quick.claimedTaskIds ?? []) : []
    }
    readonly property int dockUnitCount: dockMode ? countDockUnits() : 0
    readonly property int dockGroupCount: dockMode ? countDockGroups() : 0
    readonly property real dockFitSpacing:
        Math.max(0, dockUnitCount - dockGroupCount) * Tokens.space["1"]
        + Math.max(0, dockGroupCount - 1) * grid.spacing
        + (dockHasTaskDivider() ? 1 + Tokens.space["1"] : 0)
    readonly property int dockHorizontalTileLimit: dockUnitCount > 0
        ? Math.floor(Math.max(0, dockAvailableExtent - dockFitSpacing)
                     / dockUnitCount) : dockTileSize
    readonly property int dockVerticalTileLimit: dockZoomEnabled && !reducedMotion
        ? maxDockTileForHeight() : Math.floor(dockSurfaceHeight)
    // AGENT-CONTRACT: this value is output-local presentation geometry. It
    // must never be written back through PanelQuickConfig.
    readonly property int effectiveDockTileSize: dockMode
        ? Math.max(minimumFittedDockTileSize,
                   Math.min(dockTileSize, dockHorizontalTileLimit,
                            dockVerticalTileLimit)) : dockTileSize
    // The dock zone viewport exposes exactly this much growth envelope above
    // the shelf: the same size-derived value maxDockTileForHeight reserves
    // inside the surface and dockInputBounds masks for input/blur. Exposing
    // less clips the magnified bump at the shelf top, so the reserved
    // headroom goes unused; exposing more would paint outside the masked
    // region. Dock content is offset down by the same value so the tiles
    // stay pinned to the shelf while the viewport reaches higher.
    readonly property bool dockZoomHeadroomActive: dockMode && !vertical
        && dockZoomEnabled && !reducedMotion
    readonly property real dockZoomHeadroom: dockZoomHeadroomActive
        ? dockOverscanFor(effectiveDockTileSize) : 0
    readonly property bool dockOverflowFallback: dockMode
        && (dockHorizontalTileLimit < minimumFittedDockTileSize
            || dockVerticalTileLimit < minimumFittedDockTileSize)
    // Edit-mode drop preview (ADR-0266): while a drag hovers an accepted
    // target in this zone, the chips from the insertion point on slide aside
    // by one gap, a marker fills it, and the zone asks for the extra extent
    // so the panel makes room instead of clipping. Presentation only: the
    // profile changes on drop, through the editor transaction.
    readonly property var previewTarget: {
        const controller = liveCustomization
        if (controller === null || controller.dragActive !== true
                || controller.dropAccepted !== true) {
            return null
        }
        const target = controller.dropTarget ?? ({})
        return String(target.panelId ?? "") === String(panel.id ?? "")
            && String(target.zone ?? "") === zone ? target : null
    }
    readonly property int previewIndex: {
        if (previewTarget === null)
            return -1
        const before = String(previewTarget.beforeAppletId ?? "")
        const index = zoneApplets.findIndex(applet => String(applet.id ?? "") === before)
        return before !== "" && index >= 0 ? index : zoneApplets.length
    }
    readonly property real previewExtent: previewIndex < 0 ? 0 : Tokens.space["6"] + Tokens.space["2"]
    // The chips alone; a preview gap never reshapes a dock (dockUsesSideZones).
    readonly property real contentExtent: vertical ? grid.implicitHeight : grid.implicitWidth
    readonly property real desiredExtent: contentExtent + previewExtent
    // The extent this zone must keep to still paint every one of its applets
    // at the minimum its own manifest declares (`sizing.mainAxis.minimum`,
    // republished by the applet resolver as `runtime.mainAxisMinimum`).
    //
    // AGENT-CONTRACT: PanelContent's horizontal zone budget reserves this for
    // the zones that yield before giving a greedy zone the rest, so a wide
    // global menu can never squeeze the clock or the tray to nothing. It is
    // capped by desiredExtent because an applet that currently paints nothing
    // (an empty live strip collapses its chip to zero width) needs no
    // reservation at all; without that cap an invisible applet would steal
    // width from a visible one.
    readonly property real minimumExtent: (vertical || dockMode)
        ? 0 : Math.min(desiredExtent, declaredMinimumExtent)
    readonly property real declaredMinimumExtent: {
        let total = 0
        let count = 0
        for (const applet of zoneApplets) {
            const declared = Number((applet.runtime ?? {}).mainAxisMinimum ?? 0)
            total += Number.isFinite(declared) && declared > 0 ? declared : 0
            ++count
        }
        return total + Math.max(0, count - 1) * grid.spacing
    }
    contentWidth: vertical ? width : grid.implicitWidth + previewExtent
    contentHeight: vertical ? grid.implicitHeight + previewExtent : height
    flickableDirection: vertical ? Flickable.VerticalFlick : Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    // AGENT-GUARD: a zone never flicks in edit mode. HorizontalFlick makes
    // an overflowing zone steal a drag past the threshold, which cancels the
    // edit-mode applet drag in the middle of the gesture (ADR-0266).
    interactive: !(liveCustomization !== null && liveCustomization.editMode === true)
    clip: true
    // AGENT-GUARD: an unconditional attached scroll bar is a full-width,
    // pointer-interactive overlay across the last ~10 logical pixels of the
    // zone even while it paints at zero opacity. On a stock 30px panel that
    // is the bottom third of the 26px applet row plus its trailing edge, and
    // it consumed presses aimed at the hosted control underneath -- the
    // "clicking the lower part of a menu word does nothing" and "status icons
    // are hard to hit" defects. Each bar therefore exists only while its own
    // axis actually overflows, and stays a non-interactive indicator: the
    // zone remains scrollable by flick, wheel and keyboard reveal, which is
    // the documented overflow contract.
    T.ScrollBar.horizontal: T.ScrollBar {
        id: horizontalOverflowBar
        objectName: "panelZoneHorizontalOverflowBar"
        policy: T.ScrollBar.AsNeeded
        interactive: false
        visible: !root.vertical && root.contentWidth > root.width
        // Thin rounded token indicator: paints inside the zone's own trailing
        // padding instead of the Basic style's full-depth groove.
        implicitHeight: 4
        background: Item {}
        contentItem: Rectangle {
            implicitHeight: 4
            radius: height / 2
            color: Tokens.ready ? Tokens.fg.default
                                : (root.theme.colors ?? ({})).foreground ?? "#dddddd"
            opacity: horizontalOverflowBar.pressed ? 0.9 : 0.45
            visible: horizontalOverflowBar.size < 1
            Accessible.ignored: true
        }
    }
    T.ScrollBar.vertical: T.ScrollBar {
        id: verticalOverflowBar
        objectName: "panelZoneVerticalOverflowBar"
        policy: T.ScrollBar.AsNeeded
        interactive: false
        visible: root.vertical && root.contentHeight > root.height
        implicitWidth: 4
        background: Item {}
        contentItem: Rectangle {
            implicitWidth: 4
            radius: width / 2
            color: Tokens.ready ? Tokens.fg.default
                                : (root.theme.colors ?? ({})).foreground ?? "#dddddd"
            opacity: verticalOverflowBar.pressed ? 0.9 : 0.45
            visible: verticalOverflowBar.size < 1
            Accessible.ignored: true
        }
    }

    // Keyboard navigation must reveal the focused control inside overflow,
    // without stealing focus from applet popup windows.
    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            const item = root.Window.window.activeFocusItem
            let ancestor = item
            while (ancestor && ancestor !== grid)
                ancestor = ancestor.parent
            if (!ancestor || !item)
                return
            const point = item.mapToItem(root.contentItem, 0, 0)
            if (root.vertical) {
                root.contentY = Math.max(0, Math.min(root.contentHeight - root.height,
                    Math.min(point.y, Math.max(root.contentY, point.y + item.height - root.height))))
            } else {
                root.contentX = Math.max(0, Math.min(root.contentWidth - root.width,
                    Math.min(point.x, Math.max(root.contentX, point.x + item.width - root.width))))
            }
        }
    }

    function appletZone(applet) {
        const settings = applet.settings ?? ({});
        return settings.zone ?? "start";
    }

    // Edit-mode drop anchor: the first chip of this zone whose centre lies
    // past `mainAxisPos` (this item's coordinates) is the applet the drop
    // lands before; past the last chip the drop appends.
    function beforeAppletAt(mainAxisPos) {
        for (let index = 0; index < repeater.count; ++index) {
            const chip = repeater.itemAt(index)
            if (chip === null || chip.emptyLiveContent) {
                continue
            }
            const point = chip.mapToItem(root, chip.width / 2, chip.height / 2)
            if ((vertical ? point.y : point.x) > mainAxisPos) {
                return String(zoneApplets[index].id ?? "")
            }
        }
        return ""
    }

    function previewShift(index) {
        return previewIndex >= 0 && index >= previewIndex ? previewExtent : 0
    }

    // Where the preview gap opens along the main axis (content coordinates):
    // halfway into the spacing before the chip the drop lands before, or just
    // past the last chip that paints.
    function previewGapStart() {
        const lead = item => vertical ? item.y : item.x
        const size = item => vertical ? item.height : item.width
        const anchor = previewIndex >= 0 && previewIndex < repeater.count
            ? repeater.itemAt(previewIndex) : null
        if (anchor !== null) {
            return lead(grid) + lead(anchor) - grid.spacing / 2
        }
        let end = 0
        for (let index = 0; index < repeater.count; ++index) {
            const chip = repeater.itemAt(index)
            if (chip !== null && size(chip) > 0) {
                end = Math.max(end, lead(chip) + size(chip))
            }
        }
        return lead(grid) + end
    }

    function dockUnitsFor(applet) {
        const plugin = String(applet.plugin ?? "")
        if (["dock-task-list", "grouped-task-list", "centered-task-list",
             "task-list"].includes(plugin)) {
            return taskListAppletAccess !== null
                ? Math.max(0, Number(taskListAppletAccess.entryCount ?? 0)
                              - dockClaimedTaskIds.length) : 1
        }
        if (plugin === "quick-launch" // ADR-0268: a permanent end is one tile
                && !["file-manager", "trash"].includes((applet.settings ?? ({})).items)) {
            const quick = desktopControlsAccess !== null
                ? desktopControlsAccess.quickLaunch : null
            return quick !== null && quick !== undefined
                ? Math.max(0, Number((quick.rows ?? []).length)) : 1
        }
        return 1
    }

    function countDockUnits() {
        let count = 0
        for (const applet of zoneApplets)
            count += dockUnitsFor(applet)
        return count
    }

    function countDockGroups() {
        return zoneApplets.filter(applet => dockUnitsFor(applet) > 0).length
    }

    function dockHasTaskDivider() {
        let hasLauncher = false
        for (const applet of zoneApplets) {
            if (dockUnitsFor(applet) <= 0)
                continue
            const plugin = String(applet.plugin ?? "")
            if (isDockLauncher(applet))
                hasLauncher = true
            else if (hasLauncher && ["dock-task-list", "grouped-task-list",
                                     "centered-task-list", "task-list"].includes(plugin))
                return true
        }
        return false
    }

    function dockIconExtentFor(tileSize) {
        return Math.min(40, Math.max(16, tileSize - 8))
    }

    function dockOverscanFor(tileSize) {
        return Math.max(0, Math.ceil(dockIconExtentFor(tileSize)
                                     - tileSize / 2)) + 3
    }

    function maxDockTileForHeight() {
        for (let tileSize = 64; tileSize >= minimumFittedDockTileSize; --tileSize) {
            if (tileSize + dockOverscanFor(tileSize) <= dockSurfaceHeight)
                return tileSize
        }
        return 0
    }

    function dockInputBounds(materialX, materialY, materialWidth,
                             materialHeight, surfaceWidth, surfaceHeight,
                             contentInset) {
        const verticalOverscan = dockZoomEnabled && !reducedMotion
            ? dockOverscanFor(effectiveDockTileSize) : 0
        const horizontalOverscan = dockZoomEnabled && !reducedMotion
            ? Math.max(0, Math.ceil(dockIconExtentFor(
                effectiveDockTileSize) / 4) - contentInset) : 0
        const x = Math.max(0, materialX - horizontalOverscan)
        const y = Math.max(0, materialY - verticalOverscan)
        return Qt.rect(x, y,
            Math.min(surfaceWidth - x, materialWidth + horizontalOverscan * 2),
            Math.min(surfaceHeight - y, materialHeight + verticalOverscan))
    }

    function isDockLauncher(applet) {
        const plugin = String(applet.plugin ?? "")
        return plugin === "launcher" || plugin === "application-launcher"
            || plugin === "quick-launch"
    }

    function dockHasVisibleLauncherBefore(applet) {
        const targetIndex = zoneApplets.indexOf(applet)
        for (let index = 0; index < targetIndex; ++index) {
            if (!isDockLauncher(zoneApplets[index]))
                continue
            const chip = repeater.itemAt(index)
            if (chip !== null && !chip.emptyLiveContent)
                return true
        }
        return false
    }

    // The edit-mode drop marker, centred in the preview gap.
    Rectangle {
        objectName: "panelDropMarker"
        readonly property real center: root.previewGapStart() + root.previewExtent / 2
        visible: root.previewIndex >= 0
        width: root.vertical ? Math.max(0, root.width - 4) : 3
        height: root.vertical ? 3 : Math.max(0, root.height - root.dockZoomHeadroom - 4)
        x: root.vertical ? 2 : center - width / 2
        y: root.vertical ? center - height / 2 : root.dockZoomHeadroom + 2
        radius: 1.5
        color: Tokens.ready ? Tokens.fg.default : "#dddddd"
        Accessible.ignored: true
    }

    Grid {
        id: grid
        // Dock mode: sit below the magnification envelope so the tiles rest
        // on the shelf while the viewport's clip boundary reaches into the
        // headroom above it.
        y: root.dockZoomHeadroom
        rows: root.vertical ? -1 : root.lanes
        columns: root.vertical ? root.lanes : -1
        flow: root.vertical ? Grid.LeftToRight : Grid.TopToBottom
        spacing: 4

        Repeater {
            model: root.zoneApplets
            id: repeater

            AppletChip {
                id: chipDelegate
                required property var modelData
                required property int index

                // The drop preview's gap (previewIndex above).
                transform: Translate {
                    x: root.vertical ? 0 : root.previewShift(chipDelegate.index)
                    y: root.vertical ? root.previewShift(chipDelegate.index) : 0
                }

                // AGENT-GUARD: do not override AppletChip's zero extent for an
                // empty live applet; doing so resurrects an invisible panel slot.
                // The cross-axis chip extent subtracts the magnification
                // envelope so the chip bottom edge stays on the shelf inside
                // the taller viewport.
                height: emptyLiveContent ? 0 : root.vertical ? implicitHeight
                    : Math.max(1, (root.height - root.dockZoomHeadroom
                                   - (root.lanes - 1) * grid.spacing) / root.lanes)
                width: emptyLiveContent ? 0 : root.vertical
                    ? Math.max(1, (root.width - (root.lanes - 1) * grid.spacing) / root.lanes)
                    : implicitWidth
                vertical: root.vertical
                applet: modelData
                theme: root.theme
                liveApplets: root.liveApplets
                notificationCenterAppletAccess: root.notificationCenterAppletAccess
                audioAppletAccess: root.audioAppletAccess
                bluetoothAppletAccess: root.bluetoothAppletAccess
                networkAppletAccess: root.networkAppletAccess
                smartLightsAppletAccess: root.smartLightsAppletAccess
                voiceAppletAccess: root.voiceAppletAccess
                obsAppletAccess: root.obsAppletAccess
                gatherOverviewAccess: root.gatherOverviewAccess
                powerAppletAccess: root.powerAppletAccess
                launcherAppletAccess: root.launcherAppletAccess
                globalMenuAppletAccess: root.globalMenuAppletAccess
                clipboardAppletAccess: root.clipboardAppletAccess
                taskListAppletAccess: root.taskListAppletAccess
                statusNotifierAppletAccess: root.statusNotifierAppletAccess
                desktopControlsAccess: root.desktopControlsAccess
                dockMode: root.dockMode
                lunaMode: root.lunaMode
                dockTileSize: root.effectiveDockTileSize
                reducedMotion: root.reducedMotion
                dockZoomEnabled: root.dockZoomEnabled
                dockHasLauncherGroup: root.dockMode
                    && root.dockHasVisibleLauncherBefore(modelData)
                dockClaimedTaskIds: root.dockClaimedTaskIds

                AppletEditHandle {
                    anchors.fill: parent
                    panel: root.panel
                    applet: parent.modelData
                    controller: root.liveCustomization
                    editorHost: root.editorHost
                }
            }
        }
    }
}
