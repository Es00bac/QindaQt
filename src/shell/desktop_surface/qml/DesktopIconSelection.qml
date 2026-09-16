// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// Desktop icon selection: the selected id set, the current id, and the
// shift-range anchor, plus every gesture that mutates them.
//
// AGENT-CONTRACT: selection is keyed by ENTRY id, never by tile or by index.
// Tiles come and go as outputs adopt icons and as the listing refreshes, and
// an index-keyed selection would silently follow the wrong file after any
// directory change. `reconcile()` is the single place stale ids are dropped.
QtObject {
    id: selection

    required property var rows

    // Id set, the current entry, and the shift-range anchor. `currentId`
    // remains the single "current" id the surface contract and its tests read
    // after a plain click.
    property var selectedIds: ({})
    property string currentId: ""
    property string anchorId: ""

    function clear() {
        selectedIds = ({})
        currentId = ""
        anchorId = ""
    }
    function isSelected(entryId) { return selectedIds[entryId] !== undefined }
    function count() { return Object.keys(selectedIds).length }
    function selectedRows() {
        const picked = []
        for (const row of rows) {
            if (isSelected(row.id))
                picked.push(row)
        }
        return picked
    }
    function selectOnly(entryId) {
        const next = ({})
        next[entryId] = true
        selectedIds = next
        currentId = entryId
        anchorId = entryId
    }
    function toggle(entryId) {
        const next = Object.assign({}, selectedIds)
        if (next[entryId] !== undefined) {
            delete next[entryId]
            if (currentId === entryId)
                currentId = ""
        } else {
            next[entryId] = true
            currentId = entryId
            anchorId = entryId
        }
        selectedIds = next
    }
    function indexOfEntry(entryId) {
        for (let index = 0; index < rows.length; ++index) {
            if (rows[index].id === entryId)
                return index
        }
        return -1
    }
    function rangeTo(entryId) {
        const target = indexOfEntry(entryId)
        if (target < 0)
            return
        let anchor = indexOfEntry(anchorId)
        if (anchor < 0)
            anchor = target
        const next = Object.assign({}, selectedIds)
        for (let index = Math.min(anchor, target);
             index <= Math.max(anchor, target); ++index)
            next[rows[index].id] = true
        selectedIds = next
        currentId = entryId
    }
    function selectAll() {
        const next = ({})
        for (const row of rows)
            next[row.id] = true
        selectedIds = next
        if (rows.length > 0) {
            currentId = rows[rows.length - 1].id
            anchorId = currentId
        }
    }
    function applyMarquee(ids, modifiers) {
        if (modifiers & Qt.ControlModifier) {
            const next = Object.assign({}, selectedIds)
            for (const id of ids) {
                if (next[id] !== undefined)
                    delete next[id]
                else
                    next[id] = true
            }
            selectedIds = next
            if (ids.length > 0)
                currentId = ids[ids.length - 1]
        } else if (modifiers & Qt.ShiftModifier) {
            if (ids.length === 0)
                return
            const next = Object.assign({}, selectedIds)
            for (const id of ids)
                next[id] = true
            selectedIds = next
            currentId = ids[ids.length - 1]
        } else {
            const next = ({})
            for (const id of ids)
                next[id] = true
            selectedIds = next
            currentId = ids.length > 0 ? ids[ids.length - 1] : ""
            anchorId = currentId
        }
    }
    // Any directory change (refresh, trash, paste) revalidates selection: ids
    // that no longer name a listed entry drop out, mirroring the File Manager
    // selection reconcile contract.
    function reconcile() {
        const next = ({})
        for (const row of rows) {
            if (isSelected(row.id))
                next[row.id] = true
        }
        if (Object.keys(next).length !== Object.keys(selectedIds).length)
            selectedIds = next
        if (currentId !== "" && next[currentId] === undefined)
            currentId = ""
    }
}
