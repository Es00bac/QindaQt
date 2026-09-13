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

    // Opens the Applications popup pinned to its own default bottom-left
    // anchor (DesktopApplicationsMenu's `x`/`y` bindings). AGENT-GUARD:
    // openApplicationsMenuAtPointer() below assigns Popup.x/y imperatively,
    // which QML permanently replaces the declarative binding with — there is
    // no way to set a property "just once". Every open path that must keep
    // the fixed placement (Shift+right-click, the traditional style's
    // Applications entry) calls this first to restore the exact original
    // expressions via Qt.binding() before opening.
    function openApplicationsMenuAtDefault() {
        applicationsMenu.x = Qt.binding(function () { return 8 })
        applicationsMenu.y = Qt.binding(function () {
            return applicationsMenu.parent !== null
                ? Math.max(8, applicationsMenu.parent.height - applicationsMenu.height - 8)
                : 8
        })
        applicationsMenu.open()
    }

    // Opens the Applications popup anchored at an empty-area click's local
    // coordinates (the same coordinate space as desktopSurfaceInput's
    // `mouse.x`/`mouse.y`, since both this popup's implicit parent and that
    // MouseArea fill the same root Window content item), clamped so the
    // popup stays fully inside the surface near the right/bottom edges.
    function openApplicationsMenuAtPointer(pointerX, pointerY) {
        const bounds = applicationsMenu.parent !== null ? applicationsMenu.parent : root
        const maxX = Math.max(0, bounds.width - applicationsMenu.width)
        const maxY = Math.max(0, bounds.height - applicationsMenu.height)
        applicationsMenu.x = Math.max(0, Math.min(pointerX, maxX))
        applicationsMenu.y = Math.max(0, Math.min(pointerY, maxY))
        applicationsMenu.open()
    }

    NewFolderController {
        id: newFolderController
    }

    DesktopContentsController {
        id: desktopContents
        objectName: "desktopContentsController"
    }

    // Full-surface input, stacked UNDER the icons view: a plain Item does not
    // accept pointer events, so tile clicks reach the tiles and empty-area
    // clicks fall through to here. Empty-area left clicks clear the
    // selection, right clicks open the context menu — or the Applications
    // popup when the configured modifier is held (XFCE behavior, any style).
    // An unmodified middle click also opens the Applications popup directly
    // at the pointer (clamped inside the surface), independent of
    // style/modifier, mirroring traditional-desktop precedent.
    //
    // AGENT-GUARD: DesktopIconsView's tile MouseArea claims Qt.MiddleButton as
    // a no-op specifically so a middle click over a tile never falls through
    // to this handler. Removing that claim would make tile middle-clicks
    // reopen this popup, breaking the documented "tile middle click stays
    // inert" contract.
    MouseArea {
        objectName: "desktopSurfaceInput"
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton) {
                iconsView.clearSelection()
                return
            }
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
            contextMenu.popup()
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
    }

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

    DesktopApplicationsMenu {
        id: applicationsMenu
        objectName: "desktopApplicationsMenu"
        launcherAccess: root.launcherAccess
    }
}
