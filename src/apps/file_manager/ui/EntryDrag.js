// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Shared drag-and-drop policy for the entry views and the places sidebar.
// Internal drags carry the selection's identity maps as JSON under
// application/x-qindaqt-file-entries so drops reuse the identity-checked
// mutation batch contract; foreign drags arrive as text/uri-list and are
// always copies dispatched through ClipboardController (re-stat'ed at
// dispatch, never moved).

var internalFormat = "application/x-qindaqt-file-entries"

function mimeFor(entries) {
    const urls = []
    for (const entry of entries)
        urls.push("file://" + encodeURI(entry.path))
    const mime = {}
    mime["text/uri-list"] = urls.join("\r\n")
    mime[internalFormat] = JSON.stringify(entries)
    return mime
}

function canAccept(drag) {
    return drag.formats.indexOf(internalFormat) >= 0 || drag.urls.length > 0
}

// Refuses entries whose drop would be an already-exists no-op (same parent)
// or a recursive self-copy (destination inside the entry's own tree).
function droppableInternalEntries(jsonText, destinationDir) {
    let entries = []
    try {
        entries = JSON.parse(jsonText)
    } catch (error) {
        return []
    }
    const kept = []
    for (const entry of entries) {
        const path = entry.path || ""
        if (path.length === 0)
            continue
        if (path === destinationDir || destinationDir.startsWith(path + "/"))
            continue
        const slash = path.lastIndexOf("/")
        const parent = slash <= 0 ? "/" : path.substring(0, slash)
        if (parent === destinationDir)
            continue
        kept.push(entry)
    }
    return kept
}

// Dispatches one accepted drop. Returns the chosen Qt.DropAction, or
// Qt.IgnoreAction when nothing was dispatchable.
function dispatch(drop, destinationDir, mutationController, clipboardController) {
    if (!mutationController || !clipboardController)
        return Qt.IgnoreAction
    if (drop.formats.indexOf(internalFormat) >= 0) {
        const kept = droppableInternalEntries(drop.getDataAsString(internalFormat),
                                              destinationDir)
        if (kept.length === 0)
            return Qt.IgnoreAction
        const copy = (drop.modifiers & Qt.ControlModifier) !== 0
        if (copy)
            mutationController.copyItemsTo(kept, destinationDir)
        else
            mutationController.moveItemsTo(kept, destinationDir)
        return copy ? Qt.CopyAction : Qt.MoveAction
    }
    if (drop.urls.length > 0 &&
            clipboardController.dropUrlsInto(drop.urls, destinationDir))
        return Qt.CopyAction
    return Qt.IgnoreAction
}
