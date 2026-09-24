// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Pure main-axis geometry and drag formats for the dock's drag and drop
// (ADR-0265). Tiles sit in one row (or column) of equal slots, so every
// answer derives from the pointer's main-axis position and the slot extent.

// AGENT-CONTRACT: drag formats shared with other surfaces. The launcher rows
// (and, through W12, the File Manager's Applications view) offer
// applicationFormat carrying one desktop-entry id; the dock's own group popup
// offers memberFormat carrying {"group": dockIndex, "entryId": id} as JSON.
// Plain files and folders arrive as text/uri-list.
var applicationFormat = "application/x-qindaqt-desktop-entry-id"
var memberFormat = "application/x-qindaqt-dock-member"

// Where a pointer at `position` lands among `count` tiles. Over the middle
// half of a tile it names that tile as a merge `target` (when merging is
// allowed); anywhere else it names the insertion `gap` in [0, count].
function slotAt(position, slotExtent, tileExtent, count, mergeAllowed) {
    if (count <= 0 || !(slotExtent > 0))
        return { "gap": 0, "target": -1 }
    const index = Math.floor(position / slotExtent)
    if (mergeAllowed && index >= 0 && index < count) {
        const local = position - index * slotExtent
        if (local >= tileExtent * 0.25 && local <= tileExtent * 0.75)
            return { "gap": -1, "target": index }
    }
    return { "gap": Math.max(0, Math.min(count, Math.round(position / slotExtent))),
             "target": -1 }
}

// The visual tile at `position`, or -1 outside the strip.
function tileAt(position, slotExtent, count) {
    if (!(slotExtent > 0))
        return -1
    const index = Math.floor(position / slotExtent)
    return index >= 0 && index < count ? index : -1
}

// A visual gap as a dock-value gap. Rows carry their dock index; the end gap
// lands right after the last presented item (items the catalog cannot show
// keep their stored places).
function storageGap(rows, gap, itemCount) {
    if (gap >= 0 && gap < rows.length)
        return Number(rows[gap].index)
    return rows.length > 0 ? Number(rows[rows.length - 1].index) + 1 : itemCount
}

// Transform-only shift of tile `index`. An internal drag of tile `from`
// closes its own slot and opens the gap; an outside drag (from < 0) opens a
// gap by moving each side half a slot outward. Layout bounds never change.
function shiftFor(index, from, gap, slotExtent) {
    if (gap < 0)
        return 0
    if (from < 0)
        return index < gap ? -slotExtent / 2 : slotExtent / 2
    if (index === from)
        return 0
    if (gap > from + 1)
        return index > from && index < gap ? -slotExtent : 0
    if (gap < from)
        return index >= gap && index < from ? slotExtent : 0
    return 0
}

function canMerge(source, target) {
    const sourceKind = String(source.kind ?? "")
    const targetKind = String(target.kind ?? "")
    return sourceKind === "application"
        && (targetKind === "application" || targetKind === "group")
}
