// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The layer-shell surface itself (ADR-0290). Every Wayland fact -- which
// layer, which output, exclusive keyboard -- is set from C++ through
// LayerShellQt (PolkitOverlaySurface::configure) before this window is
// first shown, exactly as GatherOverviewWindow.qml documents for the same
// reason: LayerShellQt has no QML API and a layer surface's role must be
// decided before it maps.
Window {
    id: root

    property var viewModel: polkitViewModel

    color: "transparent"
    // AGENT-GUARD: stays false until main.cpp has configured the
    // layer-shell role and the view model actually has a request to show;
    // mapping earlier turns this into a permanent ordinary toplevel window
    // instead of an overlay (the same trap GatherOverviewWindow.qml records).
    visible: false

    PolkitDialogContent {
        anchors.fill: parent
        viewModel: root.viewModel
    }
}
