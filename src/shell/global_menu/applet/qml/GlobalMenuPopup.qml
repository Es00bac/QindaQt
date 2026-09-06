// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Window

Popup {
    id: popup

    required property var access
    required property var theme
    property int maximumDepth: 6
    property var menuStack: []
    property Item anchorItem: null
    // Incremented on every openMenu; a deferred close scheduled before a menu
    // switch (panel press deactivates the popup window before the switched
    // menu opens) must not close the freshly switched popup.
    property int openSerial: 0
    readonly property var colors: theme.colors ?? ({})
    readonly property var currentMenu:
        menuStack.length > 0 ? menuStack[menuStack.length - 1] : ({})
    readonly property var currentItems: currentMenu.children ?? []
    readonly property var popupWindow:
        contentItem !== null ? contentItem.Window.window : null
    readonly property string accessibleName:
        qsTr("%1 menu").arg(String(currentMenu.text ?? "Application"))

    function openMenu(menu, anchor) {
        if (menu === null || String(menu.kind ?? "") !== "submenu"
                || !Boolean(menu.enabled) || (menu.children ?? []).length === 0)
            return
        // AGENT-NOTE: switching anchors while open must re-anchor the LIVE
        // popup window. On Wayland the compositor owns the position and only
        // re-reads the "_q_waylandPopupAnchor*" contract on
        // xdg_popup.reposition, which QtWayland sends when the popup window's
        // geometry changes (QWaylandXdgSurface::setWindowGeometry); popup.x/y
        // never reach it (repositionPopupWindow returns early).
        const switching = opened && anchorItem !== null && anchorItem !== anchor
        ++openSerial
        anchorItem = anchor
        menuStack = [menu]
        configureNativePlacement()
        open()
        if (switching)
            requestNativeReposition()
        Qt.callLater(focusFirstItem)
    }

    // Pokes the popup window's screen position so QtWayland rebuilds the
    // xdg_popup positioner from the just-written anchor contract. The values
    // are a fallback hint only; the compositor computes the final placement.
    function requestNativeReposition() {
        const win = popupWindow
        if (win === null || parent === null || anchorItem === null)
            return
        const panelWin = parent.Window.window
        if (panelWin === null)
            return
        const scene = placementOriginFor(
            anchorItem.mapToItem(null, 0, 0), anchorItem.height, width, height,
            panelWin.width, Screen.height - panelWin.y)
        win.x = panelWin.x + scene.x
        win.y = panelWin.y + scene.y
    }

    // AGENT-CONTRACT: On Wayland the popup window position is server-side —
    // QQuickPopupPositioner::repositionPopupWindow() returns early and
    // popup.x/y never reach the compositor. QtWayland's createPositioner()
    // instead builds the xdg_popup from the popup window's REAL QObject
    // dynamic "_q_waylandPopupAnchor*" properties, read when the platform
    // surface is created (QQuickPopupWindow reparents the popup item and
    // re-evaluates popup.popupWindow before the window is shown, so writing
    // them in openMenu/onPopupWindowChanged precedes xdg_popup creation).
    // AGENT-GUARD: do NOT assign these names from QML — an unknown property
    // on a C++-created QObject wrapper becomes a JavaScript-only expando
    // (QV4::QObjectWrapper::virtualPut falls through to Object::virtualPut),
    // invisible to QtWayland; that was the ef0941e4 failure. Only the
    // NativePopupPlacement singleton's QObject::setProperty bridge is seen by
    // createPositioner().
    // The anchor rectangle handed to the native bridge: the clicked entry in
    // the panel window's coordinate system. Invalid when there is no anchor,
    // which makes NativePopupPlacement fail closed.
    function nativeAnchorRect() {
        if (anchorItem === null)
            return Qt.rect(0, 0, -1, -1)
        const topLeft = anchorItem.mapToItem(null, 0, 0)
        return Qt.rect(topLeft.x, topLeft.y, anchorItem.width,
                       anchorItem.height)
    }

    function configureNativePlacement() {
        const win = popupWindow
        if (win === null || anchorItem === null)
            return
        NativePopupPlacement.configurePopupWindow(win, nativeAnchorRect())
    }

    onPopupWindowChanged: configureNativePlacement()

    function focusFirstItem() {
        popupList.currentIndex = firstEnabledIndex(0, 1)
        popupList.forceActiveFocus(Qt.PopupFocusReason)
    }

    function firstEnabledIndex(start, direction) {
        if (currentItems.length === 0)
            return -1
        let candidate = ((start % currentItems.length) + currentItems.length)
                % currentItems.length
        for (let visited = 0; visited < currentItems.length; ++visited) {
            const item = currentItems[candidate]
            if (String(item.kind ?? "") !== "separator" && Boolean(item.enabled))
                return candidate
            candidate = (candidate + direction + currentItems.length)
                    % currentItems.length
        }
        return -1
    }

    function moveCurrent(direction) {
        const start = popupList.currentIndex < 0 ? 0
                                                 : popupList.currentIndex + direction
        popupList.currentIndex = firstEnabledIndex(start, direction)
    }

    function choose(item) {
        if (!visible || item === null || !Boolean(item.enabled))
            return
        const kind = String(item.kind ?? "")
        if (kind === "submenu") {
            if (menuStack.length >= Math.max(1, maximumDepth)
                    || (item.children ?? []).length === 0)
                return
            menuStack = menuStack.concat([item])
            Qt.callLater(focusFirstItem)
            return
        }
        if (kind === "action") {
            // AGENT-GUARD: one accepted popup gesture becomes one facade call.
            // The facade/coordinator performs the lineage guard and D-Bus
            // client deliberately does not retry uncertain Event delivery.
            access.activate(String(item.id ?? ""), String(item.generation ?? ""))
            close()
        }
    }

    function back() {
        if (menuStack.length <= 1) {
            close()
            return
        }
        menuStack = menuStack.slice(0, menuStack.length - 1)
        Qt.callLater(focusFirstItem)
    }

    function closeAfterFocusLoss() {
        const scheduledSerial = openSerial
        Qt.callLater(function() {
            if (scheduledSerial !== popup.openSerial)
                return
            if (popup.opened
                    && (!popup.activeFocus
                        || (popup.popupWindow !== null
                            && !popup.popupWindow.active)))
                popup.close()
        })
    }

    // AGENT-GUARD: anchorItem.x/y live in the anchor's parent frame (the
    // applet's entries Row/Column), while popup x/y are interpreted in the
    // popup's own parent frame and mapped across the popup Window boundary.
    // Compute placement in scene coordinates and convert back, or the popup
    // window lands displaced by the layout offset whenever the entries layout
    // does not sit at the applet origin. placementOriginFor is the pure,
    // unit-tested boundary decision: prefer the anchor's bottom-left, clamp
    // horizontally into the containing width, flip above the anchor when the
    // popup would cross availableBelow (screen bottom in window coordinates —
    // a thin panel surface is deliberately crossed, the screen edge is not).
    // This x/y path is the client-side placement used offscreen and on X11;
    // on Wayland the compositor positions the popup from the
    // "_q_waylandPopupAnchor*" contract in configureNativePlacement() and
    // never reads popup.x/y.
    function placementOriginFor(topLeft, anchorHeight, popupWidth, popupHeight,
                                boundsWidth, availableBelow) {
        let px = topLeft.x
        if (boundsWidth > 0)
            px = Math.max(0, Math.min(px, Math.max(0, boundsWidth - popupWidth)))
        let py = topLeft.y + anchorHeight
        if (availableBelow > 0 && py + popupHeight > availableBelow) {
            const above = topLeft.y - popupHeight
            if (above >= 0)
                py = above
        }
        return Qt.point(px, py)
    }

    function placementOrigin() {
        if (anchorItem === null || parent === null)
            return Qt.point(0, 0)
        const win = parent.Window.window
        const scene = placementOriginFor(
            anchorItem.mapToItem(null, 0, 0), anchorItem.height, width, height,
            win !== null ? win.width : 0,
            // AGENT-NOTE: QWindow.screen is not exposed to QML (undefined,
            // not null); the Screen attached object is the supported route.
            win !== null ? Screen.height - win.y : 0)
        return parent.mapFromItem(null, scene.x, scene.y)
    }

    objectName: "globalMenuPopup"
    // AGENT-GUARD: The production layer-shell panel deliberately rejects
    // keyboard focus. Keep this as an independent popup window so opening a
    // menu creates the keyboard-capable surface needed by the navigation
    // handlers below; an item popup silently strands focus in RuntimePanel.
    popupType: Popup.Window
    modal: false
    focus: true
    padding: 4
    width: 240
    height: Math.min(360, Math.max(28, popupList.contentHeight + 8))
    x: placementOrigin().x
    y: placementOrigin().y
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                 | Popup.CloseOnPressOutsideParent

    onClosed: {
        menuStack = []
        anchorItem = null
    }
    onActiveFocusChanged: {
        if (opened && !activeFocus)
            closeAfterFocusLoss()
    }

    Connections {
        target: popup.popupWindow

        function onActiveChanged() {
            if (popup.opened && popup.popupWindow !== null
                    && !popup.popupWindow.active)
                popup.closeAfterFocusLoss()
        }
    }

    Connections {
        target: popup.access
        function onItemsChanged() { popup.close(); popup.menuStack = [] }
        function onAvailableChanged() { if (!popup.access.available) popup.close() }
    }

    background: Rectangle {
        radius: popup.theme.cornerRadius ?? 6
        color: popup.colors.surfaceRaised ?? "#2c312e"
        border.color: popup.colors.border ?? "#3c433f"
        border.width: 1
    }

    contentItem: ListView {
        id: popupList

        objectName: "globalMenuPopupList"
        model: popup.currentItems
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationWraps: true
        Accessible.role: Accessible.PopupMenu
        Accessible.name: popup.accessibleName

        Keys.onDownPressed: event => {
            popup.moveCurrent(1)
            event.accepted = true
        }
        Keys.onUpPressed: event => {
            popup.moveCurrent(-1)
            event.accepted = true
        }
        Keys.onRightPressed: event => {
            if (currentIndex >= 0)
                popup.choose(popup.currentItems[currentIndex])
            event.accepted = true
        }
        Keys.onLeftPressed: event => {
            popup.back()
            event.accepted = true
        }
        Keys.onEscapePressed: event => {
            popup.close()
            event.accepted = true
        }
        Keys.onReturnPressed: event => {
            if (currentIndex >= 0)
                popup.choose(popup.currentItems[currentIndex])
            event.accepted = true
        }
        Keys.onEnterPressed: event => {
            if (currentIndex >= 0)
                popup.choose(popup.currentItems[currentIndex])
            event.accepted = true
        }
        Keys.onSpacePressed: event => {
            if (currentIndex >= 0)
                popup.choose(popup.currentItems[currentIndex])
            event.accepted = true
        }

        delegate: GlobalMenuPopupRow {
            currentIndex: popupList.currentIndex
            listWidth: popupList.width
            colors: popup.colors
            onActivated: item => popup.choose(item)
        }
    }
}
