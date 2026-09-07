// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Multi-select state shared by EntryList and EntryGrid. The primary (current)
// index stays owned by the host view so the existing name-based selection
// restore contract is untouched; this component only tracks the extended
// selection set and its range anchor.
QtObject {
    id: root

    property var navigationController
    // Index-keyed object (never mutated in place) so view delegates rebind
    // whenever the set changes.
    property var selected: ({})
    property int anchor: -1

    function isSelected(index) {
        return root.selected[index] === true
    }

    function count() {
        return Object.keys(root.selected).length
    }

    function selectOnly(index) {
        const next = ({})
        if (index >= 0)
            next[index] = true
        root.selected = next
        root.anchor = index
    }

    function toggle(index) {
        const next = Object.assign({}, root.selected)
        if (next[index] === true)
            delete next[index]
        else
            next[index] = true
        root.selected = next
        root.anchor = index
    }

    function rangeTo(index) {
        const total = root.navigationController.entries.length
        if (total === 0 || index < 0)
            return
        const from = root.anchor >= 0 && root.anchor < total ? root.anchor : index
        const next = ({})
        for (let i = Math.min(from, index); i <= Math.max(from, index); ++i)
            next[i] = true
        root.selected = next
    }

    function selectAll() {
        const total = root.navigationController.entries.length
        const next = ({})
        for (let i = 0; i < total; ++i)
            next[i] = true
        root.selected = next
    }

    // AGENT-GUARD: Indexes refer to the current published listing. After a
    // refresh or filter change the set must be pruned to the new length so a
    // stale index can never dispatch a mutation against the wrong entry.
    function prune() {
        const total = root.navigationController.entries.length
        const next = ({})
        for (const key of Object.keys(root.selected)) {
            const index = parseInt(key)
            if (index >= 0 && index < total)
                next[index] = true
        }
        root.selected = next
        if (root.anchor >= total)
            root.anchor = -1
    }

    // Entry maps in ascending index order; each map carries the decimal-string
    // identity fields MutationController batch dispatch requires.
    function selectedEntries() {
        const entries = root.navigationController.entries
        const indexes = Object.keys(root.selected)
            .map((key) => parseInt(key))
            .filter((index) => index >= 0 && index < entries.length)
            .sort((a, b) => a - b)
        return indexes.map((index) => entries[index])
    }
}
