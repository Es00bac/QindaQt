// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// One window owns selection; the four views are projections of this state.
QtObject {
    id: root
    required property var navigationController
    // AGENT-NOTE (ADR-0270): the one JavaScript copy of the visible listing,
    // taken once per listing change. Reading navigationController.entries
    // marshals every row again, so views, delegates and this object read
    // `entries` and `indexByKey` instead: a per-row read of the controller
    // made a 10,000-entry folder cost O(n) for every visible delegate.
    property var entries: []
    property var indexByKey: ({})
    property var selected: ({})
    property string folder: ""
    property string currentKey: ""
    property string anchorKey: ""
    readonly property int currentIndex: indexOfKey(currentKey)
    property Connections changes: Connections {
        target: root.navigationController
        function onEntriesChanged() { root.reconcile() }
        function onNavigationChanged() { root.reconcile() }
    }
    Component.onCompleted: reconcile()

    function key(entry) {
        // Recursive results can contain same-name hard links in different folders.
        return entry ? JSON.stringify([entry.path, entry.name, entry.device, entry.inode]) : ""
    }
    function indexOfKey(value) {
        const index = root.indexByKey[value]
        return index === undefined ? -1 : index
    }
    function focusIndex(index) {
        currentKey = key(root.entries[index])
    }
    function isSelected(index) {
        return selected[key(root.entries[index])] !== undefined
    }
    function count() { return Object.keys(selected).length }
    function selectOnly(index) {
        const entry = root.entries[index]
        const next = ({})
        if (entry) next[key(entry)] = entry
        selected = next
        focusIndex(index)
        anchorKey = currentKey
    }
    function toggle(index) {
        const entry = root.entries[index]
        if (!entry) return
        const next = Object.assign({}, selected)
        const id = key(entry)
        if (next[id] !== undefined) delete next[id]
        else next[id] = entry
        selected = next
        focusIndex(index)
        anchorKey = currentKey
    }
    function rangeTo(index, additive = false) {
        const entries = root.entries
        if (index < 0 || index >= entries.length) return
        let anchor = indexOfKey(anchorKey)
        if (anchor < 0) {
            anchor = currentIndex >= 0 ? currentIndex : index
            anchorKey = key(entries[anchor])
        }
        const next = additive ? Object.assign({}, selected) : ({})
        for (let i = Math.min(anchor, index); i <= Math.max(anchor, index); ++i)
            next[key(entries[i])] = entries[i]
        selected = next
        focusIndex(index)
    }
    // Rubber-band results arrive as a raw index array (view-order
    // intersection, already sorted). Modifiers match every other selection
    // gesture: Shift unions into the current set, Control toggles each hit,
    // and no modifier replaces the set. An empty array with no modifier is
    // an empty-space click and clears the selection.
    function applyIndexSet(indexes, modifiers) {
        const entries = root.entries
        const valid = indexes.filter(index => index >= 0 && index < entries.length)
        if (modifiers & Qt.ControlModifier) {
            const next = Object.assign({}, selected)
            for (const index of valid) {
                const id = key(entries[index])
                if (next[id] !== undefined) delete next[id]
                else next[id] = entries[index]
            }
            selected = next
        } else if (modifiers & Qt.ShiftModifier) {
            const next = Object.assign({}, selected)
            for (const index of valid) next[key(entries[index])] = entries[index]
            selected = next
        } else {
            const next = ({})
            for (const index of valid) next[key(entries[index])] = entries[index]
            selected = next
        }
        if (valid.length > 0) {
            focusIndex(valid[valid.length - 1])
            anchorKey = currentKey
        }
    }
    function selectAll() {
        const next = ({})
        for (const entry of root.entries) next[key(entry)] = entry
        selected = next
    }
    function moveTo(index, modifiers) {
        if (modifiers & Qt.ShiftModifier) rangeTo(index, Boolean(modifiers & Qt.ControlModifier))
        else if (modifiers & Qt.ControlModifier) focusIndex(index)
        else selectOnly(index)
    }
    // The one click policy every view's entries share (ADR-0270): a plain
    // click selects only this entry, Shift extends from the anchor (adding
    // the range with Control), Control toggles, and a right-click on an entry
    // that is already selected keeps the batch for the context menu.
    function click(index, button, modifiers) {
        if (button === Qt.RightButton && isSelected(index)) {
            // keep the batch
        } else if (modifiers & Qt.ShiftModifier) {
            rangeTo(index, Boolean(modifiers & Qt.ControlModifier))
        } else if (modifiers & Qt.ControlModifier) {
            toggle(index)
        } else {
            selectOnly(index)
        }
        focusIndex(index)
    }
    // A touch-and-hold on an entry targets it for the context menu, keeping
    // a batch that already contains it.
    function target(index) {
        if (!isSelected(index)) selectOnly(index)
        focusIndex(index)
    }
    function reconcile() {
        // AGENT-GUARD: Navigation changes the authority of every row. Clear
        // selection even if the destination has identical names or inodes.
        if (folder !== navigationController.currentPath) {
            folder = navigationController.currentPath
            selected = ({})
            currentKey = ""
            anchorKey = ""
        }
        const entries = navigationController.entries
        const byKey = ({})
        for (let i = 0; i < entries.length; ++i)
            byKey[key(entries[i])] = i
        root.indexByKey = byKey
        root.entries = entries
        const next = ({})
        for (const entry of entries) {
            const id = key(entry)
            if (selected[id] !== undefined) next[id] = selected[id]
        }
        selected = next
        if (indexOfKey(currentKey) < 0) focusIndex(0)
        if (indexOfKey(anchorKey) < 0) anchorKey = currentKey
    }
    function selectedEntries() {
        // AGENT-CONTRACT: Retain listing-time identity for mutation checks.
        // Never refresh a selected entry's identity from a replacement row.
        if (folder !== navigationController.currentPath) return []
        return root.entries.filter(entry => selected[key(entry)] !== undefined)
            .map(entry => selected[key(entry)])
    }
}
