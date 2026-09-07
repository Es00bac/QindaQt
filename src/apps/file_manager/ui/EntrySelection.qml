// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// One window owns selection; list/grid are projections of this state.
QtObject {
    id: root
    required property var navigationController
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
        return entry ? JSON.stringify([entry.name, entry.device, entry.inode]) : ""
    }
    function indexOfKey(value) {
        const entries = navigationController.entries
        for (let i = 0; i < entries.length; ++i)
            if (key(entries[i]) === value) return i
        return -1
    }
    function focusIndex(index) {
        currentKey = key(navigationController.entries[index])
    }
    function isSelected(index) {
        return selected[key(navigationController.entries[index])] !== undefined
    }
    function count() { return Object.keys(selected).length }
    function selectOnly(index) {
        const entry = navigationController.entries[index]
        const next = ({})
        if (entry) next[key(entry)] = entry
        selected = next
        focusIndex(index)
        anchorKey = currentKey
    }
    function toggle(index) {
        const entry = navigationController.entries[index]
        if (!entry) return
        const next = Object.assign({}, selected)
        const id = key(entry)
        if (next[id] !== undefined) delete next[id]
        else next[id] = entry
        selected = next
        focusIndex(index)
        anchorKey = currentKey
    }
    function rangeTo(index) {
        const entries = navigationController.entries
        if (index < 0 || index >= entries.length) return
        let anchor = indexOfKey(anchorKey)
        if (anchor < 0) {
            anchor = currentIndex >= 0 ? currentIndex : index
            anchorKey = key(entries[anchor])
        }
        const next = ({})
        for (let i = Math.min(anchor, index); i <= Math.max(anchor, index); ++i)
            next[key(entries[i])] = entries[i]
        selected = next
        focusIndex(index)
    }
    function selectAll() {
        const next = ({})
        for (const entry of navigationController.entries) next[key(entry)] = entry
        selected = next
    }
    function moveTo(index, modifiers) {
        if (modifiers & Qt.ShiftModifier) rangeTo(index)
        else if (modifiers & Qt.ControlModifier) focusIndex(index)
        else selectOnly(index)
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
        const next = ({})
        for (const entry of navigationController.entries) {
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
        return navigationController.entries.filter(entry => selected[key(entry)] !== undefined)
            .map(entry => selected[key(entry)])
    }
}
