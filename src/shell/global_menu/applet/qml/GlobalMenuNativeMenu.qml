// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic

// AGENT-GUARD: QindaQt owns the presentation over the Basic controls. An
// ambient desktop style can add hover/focus policy and opening transitions
// that break our keyboard contract even after the visual delegates are replaced.
Basic.Menu {
    id: nativeMenu

    required property var menuData
    required property var access
    required property var theme
    property int sourceIndex: -1
    property int depth: 0
    property int maximumDepth: 6
    property bool interactive: true
    property bool projectionReady: false
    // Top-level placement inputs. A non-empty panelEdge ("top", "bottom",
    // "left", "right") wins over the hosting RuntimePanel's edge; vertical
    // selects the fallback axis for hosts without a panel model.
    property string panelEdge: ""
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    // AGENT-NOTE: Qt's Wayland positioner slides a popup but never resizes it,
    // so a menu with more entries than the output can show must cap its own
    // implicit height. QQuickPopupWindow also sizes from implicitHeight when
    // the list changes; capping only height is undone by that native resize.
    // Top and bottom panels reserve the bar's thickness because the
    // popup opens flush against it. Submenus receive the parent menu's value in
    // populate(): their parent item sits in the parent popup window, not the
    // panel. The cap makes entryList interactive, which gives wheel scrolling;
    // edgeScroll adds scrolling by hovering near the top or bottom edge.
    property real verticalRoom: {
        const anchor = parent
        if (depth > 0 || anchor === null)
            return -1
        const edge = panelEdgeFor(anchor)
        const hostWindow = anchor.Window.window
        const reserved = hostWindow !== null && (edge === "top" || edge === "bottom")
            ? hostWindow.height : 0
        return anchor.Screen.height - reserved
    }

    objectName: "globalMenuNativeMenu"
    title: String(menuData.text ?? "")
    enabled: interactive && Boolean(menuData.enabled)
        && (menuData.children ?? []).length > 0
    popupType: Popup.Window
    modal: false
    focus: true
    padding: 4
    width: 240
    readonly property real naturalHeight: Math.max(
        implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding)
    implicitHeight: verticalRoom > 0 ? Math.min(naturalHeight, verticalRoom) : naturalHeight
    height: implicitHeight
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                 | Popup.CloseOnReleaseOutsideParent

    // AGENT-CONTRACT: top-level placement reuses desktop controls' pure
    // ControlPopupFrame contract (docs/wiki/shell/desktop-controls.md, "Popup
    // placement"): flush with the triggering item's leading edge, below it,
    // above it on a bottom panel, beside it on side panels, then slid along the
    // panel axis to stay on the output. tst_GlobalMenuPopupPlacement.qml pins the
    // same vectors as tst_desktop_controls_qml_menus.cpp.
    // AGENT-GUARD: `anchorPosition` is window-local on purpose. A Wayland
    // layer-shell client never learns where its window sits, so clamping happens
    // only along the panel axis and the compositor's popup positioner remains
    // the backstop.
    function placementFor(anchorPosition, anchorWidth, anchorHeight, popupWidth, popupHeight,
                          edge, boundsWidth, boundsHeight) {
        const sideways = edge === "left" || edge === "right"
        let px = edge === "left" ? anchorWidth : (edge === "right" ? -popupWidth : 0)
        let py = sideways ? 0 : (edge === "bottom" ? -popupHeight : anchorHeight)
        if (sideways)
            py += slideOffset(anchorPosition.y + py, popupHeight, boundsHeight)
        else
            px += slideOffset(anchorPosition.x + px, popupWidth, boundsWidth)
        return Qt.point(px, py)
    }

    // Shift keeping [start, start + extent] inside [0, limit], preferring the
    // start edge when the popup is larger than the output.
    function slideOffset(start, extent, limit) {
        if (!(limit > 0))
            return 0
        return Math.max(0, Math.min(start, limit - extent)) - start
    }

    function panelEdgeFor(anchor) {
        if (panelEdge.length > 0)
            return panelEdge
        const hostWindow = anchor.Window.window
        const hostPanel = hostWindow !== null ? hostWindow.panel : undefined
        const edge = hostPanel ? String(hostPanel.edge ?? "") : ""
        if (edge === "top" || edge === "bottom" || edge === "left" || edge === "right")
            return edge
        // Hosts without a panel model (desktop surface, previews, tests): open
        // away from the nearer output edge across the bar.
        const center = anchor.mapToItem(null, anchor.width / 2, anchor.height / 2)
        if (vertical)
            return anchor.Screen.width > 0 && center.x > anchor.Screen.width / 2 ? "right" : "left"
        return anchor.Screen.height > 0 && center.y > anchor.Screen.height / 2 ? "bottom" : "top"
    }

    // AGENT-GUARD: MenuBar opens a top-level menu with itself as the menu's
    // parent MenuBarItem and requests an origin below that item. Replace only
    // that requested origin, before the window is shown; Qt still owns opening,
    // switching, focus, submenu cascade, and dismissal. Submenus keep Qt's
    // cascade placement.
    function placeAtMenuBarItem() {
        const anchor = parent
        if (depth !== 0 || anchor === null || anchor.menu !== nativeMenu)
            return
        const placed = placementFor(anchor.mapToItem(null, 0, 0), anchor.width, anchor.height,
                                    width, height, panelEdgeFor(anchor),
                                    anchor.Screen.width, anchor.Screen.height)
        x = placed.x
        y = placed.y
    }

    // The menu tree is an immutable facade publication. Re-publication
    // may reuse its top-level Menu, so menuData changes rebuild descendants
    // before an obsolete generation can be invoked. Qt owns popup state.
    function populate() {
        const children = menuData.children ?? []
        for (let index = 0; index < children.length; ++index) {
            const entry = children[index]
            const kind = String(entry.kind ?? "action")
            if (kind === "submenu" && depth + 1 < maximumDepth) {
                const component = Qt.createComponent(
                    Qt.resolvedUrl("GlobalMenuNativeMenu.qml"),
                    Component.PreferSynchronous)
                if (component.status !== Component.Ready) {
                    console.warn("Unable to create application submenu:",
                                 component.errorString())
                    continue
                }
                const submenu = component.createObject(nativeMenu, {
                    "menuData": entry,
                    "access": access,
                    "theme": Qt.binding(function() { return nativeMenu.theme }),
                    "depth": depth + 1,
                    "maximumDepth": maximumDepth,
                    "verticalRoom": Qt.binding(function() { return nativeMenu.verticalRoom }),
                    "interactive": Qt.binding(function() {
                        return nativeMenu.interactive
                    })
                })
                if (submenu !== null)
                    nativeMenu.insertMenu(nativeMenu.count, submenu)
                continue
            }
            if (kind === "separator") {
                const separator = separatorComponent.createObject(nativeMenu.contentItem)
                if (separator !== null)
                    nativeMenu.insertItem(nativeMenu.count, separator)
                continue
            }
            const item = actionComponent.createObject(nativeMenu.contentItem, {
                "entryData": entry,
                "access": access,
                "colors": Qt.binding(function() { return nativeMenu.colors }),
                "interactive": kind !== "submenu"
                    ? Qt.binding(function() { return nativeMenu.interactive }) : false
            })
            if (item !== null)
                nativeMenu.insertItem(nativeMenu.count, item)
        }
    }

    function clearProjection() {
        while (count > 0) {
            const submenu = menuAt(0)
            if (submenu !== null)
                removeMenu(submenu)
            else
                removeItem(itemAt(0))
        }
    }

    function rebuildProjection() {
        if (!projectionReady)
            return
        // AGENT-GUARD: Instantiator may reuse a top-level Menu delegate when
        // a new facade publication has the same shape. Dismiss before
        // replacing descendants so no queued gesture can invoke the prior
        // publication generation, then rebuild synchronously from new truth.
        dismiss()
        clearProjection()
        populate()
    }

    onAboutToShow: placeAtMenuBarItem()
    onMenuDataChanged: rebuildProjection()
    Component.onCompleted: {
        projectionReady = true
        populate()
    }

    // AGENT-GUARD: MenuItem.menu is null until Qt inserts the delegate. Use
    // the unambiguous root id during construction, not the inherited property.
    delegate: GlobalMenuNativeMenuItem {
        entryData: subMenu !== null ? subMenu.menuData : ({})
        access: nativeMenu.access
        colors: nativeMenu.colors
        interactive: nativeMenu.interactive
    }

    // The Basic style's list, which is interactive (wheel-scrollable) only
    // while the entries exceed the capped height, plus edge hover scrolling.
    contentItem: ListView {
        id: entryList

        // Hovering within edgeZone pixels of the top or bottom edge scrolls
        // edgeStep pixels per edgeScroll tick toward that edge.
        readonly property int edgeZone: 24
        readonly property int edgeStep: 4
        readonly property int edgeDirection: {
            if (!interactive || !edgeHover.hovered)
                return 0
            const y = edgeHover.point.position.y
            if (y < edgeZone)
                return -1
            return y > height - edgeZone ? 1 : 0
        }

        implicitHeight: contentHeight
        model: nativeMenu.contentModel
        interactive: contentHeight > height
        clip: true
        currentIndex: nativeMenu.currentIndex
        boundsBehavior: Flickable.StopAtBounds

        ScrollIndicator.vertical: Basic.ScrollIndicator {
            palette.mid: nativeMenu.colors.textMuted ?? "#a9afa9"
        }

        HoverHandler {
            id: edgeHover
        }

        Timer {
            id: edgeScroll
            interval: 16
            repeat: true
            running: (entryList.edgeDirection < 0 && !entryList.atYBeginning)
                || (entryList.edgeDirection > 0 && !entryList.atYEnd)
            onTriggered: {
                const minY = entryList.originY
                const maxY = minY + Math.max(0, entryList.contentHeight - entryList.height)
                entryList.contentY = Math.max(minY, Math.min(maxY,
                    entryList.contentY + entryList.edgeDirection * entryList.edgeStep))
            }
        }
    }

    background: Rectangle {
        radius: nativeMenu.theme.cornerRadius ?? 6
        color: nativeMenu.colors.surfaceRaised ?? "#2c312e"
        border.color: nativeMenu.colors.border ?? "#3c433f"
        border.width: 1
    }

    Component {
        id: actionComponent
        GlobalMenuNativeMenuItem {}
    }

    Component {
        id: separatorComponent
        Basic.MenuSeparator {
            objectName: "globalMenuNativeSeparator"
            implicitHeight: 9
            contentItem: Rectangle {
                implicitHeight: 1
                color: nativeMenu.colors.border ?? "#3c433f"
            }
        }
    }
}
