// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Shared presentation-only keyboard behavior; EntrySelection remains the
// authority for identity, selection ranges and mutation snapshots. Counts
// come from the selection's cached listing (ADR-0270), never from the view,
// whose rows may include headings.
Item {
    id: root
    required property var view
    required property var selection
    required property var navigationController
    property int columns: 1
    property real rowHeight: 44
    property string prefix: ""
    signal contextMenuRequested()
    // ADR-0272: Space with no name being typed. The owning view answers it
    // with the catalog's "file.quick-look", so Space and the menu item share
    // one route.
    signal quickLookRequested()

    Timer { id: prefixExpiry; interval: 1000; onTriggered: root.prefix = "" }

    // A view whose rows are not its entries (the Details table's group
    // headings, the Gallery's strip) declares revealIndex(entryIndex); a
    // plain ListView or GridView is positioned directly.
    function ensureCurrentVisible() {
        if (view.visible && selection.currentIndex >= 0) {
            if (typeof view.revealIndex === "function") {
                view.revealIndex(selection.currentIndex)
                return
            }
            view.forceLayout()
            view.positionViewAtIndex(selection.currentIndex, ListView.Contain)
        }
    }
    function scheduleReveal() { Qt.callLater(ensureCurrentVisible) }

    function handle(event) {
        if (event.key === Qt.Key_Backspace) {
            navigationController.goUp()
        } else if ([Qt.Key_Up, Qt.Key_Down, Qt.Key_Left, Qt.Key_Right,
                    Qt.Key_Home, Qt.Key_End, Qt.Key_PageUp, Qt.Key_PageDown].indexOf(event.key) >= 0) {
            const count = selection.entries.length
            const page = Math.max(1, Math.floor(view.height / rowHeight)) * columns
            let target = selection.currentIndex
            if (event.key === Qt.Key_Home) target = 0
            else if (event.key === Qt.Key_End) target = count - 1
            else if (event.key === Qt.Key_Up) target -= columns
            else if (event.key === Qt.Key_Down) target += columns
            else if (event.key === Qt.Key_Left) target -= 1
            else if (event.key === Qt.Key_Right) target += 1
            else if (event.key === Qt.Key_PageUp) target -= page
            else target += page
            target = Math.max(0, Math.min(count - 1, target))
            selection.moveTo(target, event.modifiers)
            ensureCurrentVisible()
            prefix = ""
        } else if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
            selection.toggle(selection.currentIndex)
        } else if (event.key === Qt.Key_Space && prefix.length === 0) {
            // Finder's Space: preview the selection, or the focused entry
            // when nothing is selected (Ctrl+arrows move focus alone).
            if (selection.currentIndex >= 0 && selection.selectedEntries().length === 0)
                selection.selectOnly(selection.currentIndex)
            quickLookRequested()
        } else if (event.key === Qt.Key_Menu
                   || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
            contextMenuRequested()
        } else if (event.text.length > 0
                   && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
                   && event.text.charCodeAt(0) >= 32) {
            selectPrefix(event.text)
        } else {
            return
        }
        event.accepted = true
    }

    // Type-to-select (every view): a space typed while a name is being typed
    // belongs to the name ("my notes"), and only then; see Key_Space above.
    function selectPrefix(text) {
        const entries = selection.entries
        const typed = text.toLocaleLowerCase()
        // Repeated initial letters cycle matches; extending a prefix keeps
        // the current matching item, so typing "doc" does not skip it.
        const cycle = prefix === typed
        prefix = cycle ? typed : prefix + typed
        prefixExpiry.restart()
        const start = Math.max(0, selection.currentIndex + (cycle || prefix === typed ? 1 : 0))
        for (let offset = 0; offset < entries.length; ++offset) {
            const index = (start + offset) % entries.length
            if (entries[index].name.toLocaleLowerCase().startsWith(prefix)) {
                selection.selectOnly(index)
                ensureCurrentVisible()
                return
            }
        }
    }

    Connections {
        target: root.navigationController
        function onNavigationChanged() { root.prefix = "" }
    }
}
