// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Root of the per-output desktop surface window (ADR-0125): place icons over
// the wallpaper, a styled right-click context menu, and a modifier-gated
// Applications popup. The controller keeps this window hidden until its
// layer-shell role is assigned.
//
// AGENT-GUARD: this window shares the wallpaper's background layer and the
// exact `desktop` scope (requested by the controller). Never set
// `visible: true` here, change `flags`, or paint an opaque background: the
// surface stays transparent over the wallpaper and must never become an
// ordinary toplevel.
Window {
    id: root

    // Resolved desktop applet inventory (ResolvedAppletInstance::toVariantMap
    // shape); the "desktop-icons" entry carries this applet's settings.
    required property var applets
    // Borrowed DesktopControlsAccess and LauncherAppletController facades.
    // Either may be null (fail-closed grant); every consumer disables itself
    // instead of dereferencing.
    required property var access
    required property var launcherAccess
    required property string screenName

    visible: false
    color: "transparent"
    title: qsTr("QindaQt desktop %1").arg(screenName)
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus

    // Settings of the first resolved "desktop-icons" applet instance, with
    // manifest defaults applied for anything missing or malformed.
    readonly property var appletSettings: {
        for (let i = 0; i < applets.length; ++i) {
            if (String(applets[i].id) === "desktop-icons") {
                return applets[i].settings ?? {}
            }
        }
        return {}
    }
    readonly property string contextMenuStyle: {
        const requested = appletSettings.contextMenuStyle ?? "windows"
        return requested === "mac" || requested === "traditional"
               ? requested : "windows"
    }
    // Qt.NoModifier disables the modifier-gated Applications menu entirely.
    readonly property int applicationsModifier: {
        switch (appletSettings.applicationsMenuModifier ?? "shift") {
        case "ctrl": return Qt.ControlModifier
        case "alt": return Qt.AltModifier
        case "shift": return Qt.ShiftModifier
        default: return Qt.NoModifier
        }
    }

    // Window-backed popups are positioned by their parent item's xdg anchor,
    // not Popup.x/y, on Wayland. Each popup is therefore parented to a 1x1
    // positioner whose top-right corner is the requested popup origin.
    // Keeping the anchor itself inside the desktop surface preserves a valid
    // xdg_positioner near output edges.
    function positionPopupAnchor(anchor, pointerX, pointerY, popupWidth,
                                 popupHeight) {
        const requestedX = Math.max(0, Math.min(pointerX,
            Math.max(0, root.width - popupWidth)))
        const requestedY = Math.max(0, Math.min(pointerY,
            Math.max(0, root.height - popupHeight)))
        anchor.x = Math.max(0, requestedX - anchor.width)
        anchor.y = requestedY
    }

    function openContextMenuAtPointer(pointerX, pointerY) {
        positionPopupAnchor(contextMenuAnchor, pointerX, pointerY,
                            contextMenu.width, contextMenu.height)
        contextMenu.popup()
    }

    // Opens the Applications popup at its fixed bottom-left default. The
    // traditional-style entry has no click coordinates of its own, and the
    // modifier path intentionally keeps this documented placement.
    function openApplicationsMenuAtDefault() {
        positionPopupAnchor(applicationsMenuAnchor, 8,
                            Math.max(8, root.height - applicationsMenu.height - 8),
                            applicationsMenu.width, applicationsMenu.height)
        applicationsMenu.open()
    }

    // Opens the Applications popup anchored at an empty-area click's local
    // coordinates (the same coordinate space as desktopSurfaceInput's
    // `mouse.x`/`mouse.y`, since both this popup's implicit parent and that
    // MouseArea fill the same root Window content item), clamped so the
    // popup stays fully inside the surface near the right/bottom edges.
    function openApplicationsMenuAtPointer(pointerX, pointerY) {
        positionPopupAnchor(applicationsMenuAnchor, pointerX, pointerY,
                            applicationsMenu.width, applicationsMenu.height)
        applicationsMenu.open()
    }

    NewFolderController {
        id: newFolderController
    }

    DesktopContentsController {
        id: desktopContents
        objectName: "desktopContentsController"
    }

    DesktopIconLayoutStore {
        id: desktopIconLayout
        objectName: "desktopIconLayoutStore"
    }

    // Full-surface input, stacked UNDER the icons view: a plain Item does not
    // accept pointer events, so tile clicks reach the tiles and empty-area
    // clicks fall through to here — except left clicks, which the icons
    // view's marquee band claims first (an empty-area click that drags
    // marquee-selects; one that does not drag clears the selection). This
    // handler keeps the remaining empty-area gestures: right clicks open the
    // context menu — or the Applications popup when the configured modifier
    // is held (XFCE behavior, any style) — and an unmodified middle click
    // opens the Applications popup directly at the pointer (clamped inside
    // the surface), independent of style/modifier, mirroring
    // traditional-desktop precedent.
    //
    // AGENT-GUARD: DesktopIconsView's tile MouseArea claims Qt.MiddleButton as
    // a no-op specifically so a middle click over a tile never falls through
    // to this handler. Removing that claim would make tile middle-clicks
    // reopen this popup, breaking the documented "tile middle click stays
    // inert" contract.
    MouseArea {
        objectName: "desktopSurfaceInput"
        anchors.fill: parent
        acceptedButtons: Qt.RightButton | Qt.MiddleButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.MiddleButton) {
                // Fail closed (no popup) when the borrowed launcher facade is
                // absent, rather than opening an empty popup for an
                // unmodified gesture — same posture as the context menu's
                // needsLauncher entry gating.
                if (root.launcherAccess !== null) {
                    root.openApplicationsMenuAtPointer(mouse.x, mouse.y)
                }
                return
            }
            if (root.applicationsModifier !== Qt.NoModifier
                    && (mouse.modifiers & root.applicationsModifier)) {
                root.openApplicationsMenuAtDefault()
                return
            }
            root.openContextMenuAtPointer(mouse.x, mouse.y)
        }
    }

    // The view fills the surface and anchors its flow against the edge the
    // placement setting picks.
    DesktopIconsView {
        id: iconsView
        objectName: "desktopIconsView"
        anchors.fill: parent
        settings: root.appletSettings
        contents: desktopContents
        layoutStore: desktopIconLayout
        screenName: root.screenName
    }

    Item {
        id: contextMenuAnchor
        objectName: "desktopContextMenuAnchor"
        width: 1
        height: 1

        DesktopContextMenu {
            id: contextMenu
            objectName: "desktopContextMenu"
            style: root.contextMenuStyle
            placesAccess: root.access !== null && root.access.places !== null
                          ? root.access.places : null
            launcherAccess: root.launcherAccess
            newFolder: newFolderController
            iconsView: iconsView
            onApplicationsRequested: root.openApplicationsMenuAtDefault()
        }
    }

    Item {
        id: applicationsMenuAnchor
        objectName: "desktopApplicationsMenuAnchor"
        width: 1
        height: 1

        DesktopApplicationsMenu {
            id: applicationsMenu
            objectName: "desktopApplicationsMenu"
            launcherAccess: root.launcherAccess
        }
    }
}
