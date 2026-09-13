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
    // An unmodified middle click also opens the Applications popup directly,
    // independent of style/modifier, mirroring traditional-desktop precedent.
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
                    applicationsMenu.open()
                }
                return
            }
            if (root.applicationsModifier !== Qt.NoModifier
                    && (mouse.modifiers & root.applicationsModifier)) {
                applicationsMenu.open()
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
        onApplicationsRequested: applicationsMenu.open()
    }

    DesktopApplicationsMenu {
        id: applicationsMenu
        objectName: "desktopApplicationsMenu"
        launcherAccess: root.launcherAccess
    }
}
