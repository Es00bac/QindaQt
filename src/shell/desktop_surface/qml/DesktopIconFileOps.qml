// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The desktop icon view's file operations, split from DesktopIconsView (its
// QML shape limit): every one goes through the DesktopContentsController
// boundary, and opening a standard icon goes through DesktopPlacesController.
//
// AGENT-GUARD (ADR-0282): a standard icon (Home, Trash, ...) is not a file on
// the Desktop. Every file operation takes only the Desktop-folder rows; the
// contents controller refuses a whole batch that names anything its listing
// did not report, so one selected Home icon would otherwise block the rest,
// and a place must never be renamed, cut, copied, trashed or pinned.
QtObject {
    id: ops

    required property var selection
    required property var contents
    property var places: null
    property var dockAccess: null

    function isPlaceEntry(entryId) { return places !== null && places.isPlace(entryId) }
    function open(entryId) {
        if (isPlaceEntry(entryId))
            places.open(entryId)
        else
            contents.open(entryId)
    }
    function fileRows(rows) { return rows.filter(row => row.isPlace !== true) }
    function trashRows(rows) {
        const picked = fileRows(rows)
        if (picked.length > 0)
            contents.trashEntries(picked)
    }
    function trashSelection() { trashRows(selection.selectedRows()) }
    function cutSelection() {
        const picked = fileRows(selection.selectedRows())
        if (picked.length > 0)
            contents.cutSelection(picked)
    }
    function copySelection() {
        const picked = fileRows(selection.selectedRows())
        if (picked.length > 0)
            contents.copySelection(picked)
    }
    // The whole selection joins the dock as one edit (the dock refuses a
    // second write while the first is saving).
    function addSelectionToDock() {
        if (dockAccess === null)
            return
        const paths = fileRows(selection.selectedRows()).map(row => String(row.path))
        if (paths.length > 0)
            dockAccess.addPaths(paths)
    }
}
