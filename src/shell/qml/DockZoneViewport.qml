// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// Placement arithmetic shared by PanelContent's three dock zone viewports.
// AGENT-CONTRACT: a dock zone viewport exposes exactly the size-derived
// magnification envelope above a shelf pinned to the panel's bottom edge —
// the same value maxDockTileForHeight reserves inside the surface and
// dockInputBounds/PanelSurfaceBlur mask and blur (PanelAppletRow.dockZoomHeadroom,
// wiki shell/dock-interactions.md). Exposing less clips the magnified bump
// at the shelf top, so the reserved headroom goes unused; exposing more
// paints outside the masked region.
QtObject {
    // Shelf-top y of a dock zone viewport: the viewport reaches headroom
    // above the shelf while its bottom edge stays on the panel's bottom edge.
    function shelfY(surfaceHeight, tileSize, headroom) {
        return surfaceHeight - tileSize - headroom
    }

    // Cross-axis extent of a dock zone viewport: tile row plus envelope.
    function shelfExtent(tileSize, headroom) {
        return tileSize + headroom
    }
}
