// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

// The window that carries the gather overview on one output (ADR-0232).
//
// AGENT-CONTRACT: this file declares presentation and nothing else. Every
// Wayland fact about the window - which layer it sits in, which output it
// belongs to, whether it takes the keyboard - is set from C++ through
// LayerShellQt, exactly as the desktop surface does, because LayerShellQt has
// no QML API and a layer surface's role must be decided before it maps.
//
// The window stays alive for the whole session and is shown and hidden, rather
// than created and destroyed per invocation. Creating a layer surface costs a
// roundtrip and a first-frame wait, and the overview is a thing the user
// flicks in and out of; a session that pays that cost every time feels like
// the sluggish start menu this desktop is trying to stop being.
Window {
    id: root

    // The controller that owns the projection and the open state.
    required property var controller
    // The output work area in desktop-logical coordinates, so a surface on a
    // secondary output draws its frames in its own coordinates.
    property rect workArea: Qt.rect(0, 0, 0, 0)

    readonly property var projection: controller ? controller.projection : null

    // Transparent: the surface paints its own scrim in the popup material, so
    // the window must not paint a second ground underneath it.
    color: "transparent"
    // AGENT-GUARD: `flags` is set from C++ before the layer-shell role is
    // requested, and `visible` is driven by the controller through the
    // composition. Setting either here races the role assignment, and a
    // layer surface that maps before its role is assigned becomes a
    // permanent ordinary toplevel - the same trap DesktopSurface.qml records.
    visible: false

    GatherOverviewSurface {
        id: surface
        objectName: "gatherOverviewSurface"
        anchors.fill: parent
        projection: root.projection
        origin: Qt.point(root.workArea.x, root.workArea.y)

        onActivated: (item) => root.controller.activate(item)
        onScrollRequested: (delta) => root.controller.scrollBy(delta)
        onDismissRequested: root.controller.close()
    }

    // The surface is keyboard-reachable only while it is up, so Escape is
    // bound here rather than as a global shortcut: a global Escape would fight
    // every dialog in the session.
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.visible
        onActivated: root.controller.close()
    }
}
