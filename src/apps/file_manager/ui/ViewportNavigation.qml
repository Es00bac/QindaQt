// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Shared presentation-only keyboard behavior; EntrySelection remains the
// authority for identity, selection ranges and mutation snapshots.
Item {
    id: root
    required property var view
    required property var selection
    required property var navigationController
    property int columns: 1
    property real rowHeight: 44
    property string prefix: ""
    signal contextMenuRequested()

    Timer { id: prefixExpiry; interval: 1000; onTriggered: root.prefix = "" }

    function ensureCurrentVisible() {
        if (view.visible && selection.currentIndex >= 0) {
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
            const page = Math.max(1, Math.floor(view.height / rowHeight)) * columns
            let target = selection.currentIndex
            if (event.key === Qt.Key_Home) target = 0
            else if (event.key === Qt.Key_End) target = view.count - 1
            else if (event.key === Qt.Key_Up) target -= columns
            else if (event.key === Qt.Key_Down) target += columns
            else if (event.key === Qt.Key_Left) target -= 1
            else if (event.key === Qt.Key_Right) target += 1
            else if (event.key === Qt.Key_PageUp) target -= page
            else target += page
            target = Math.max(0, Math.min(view.count - 1, target))
            selection.moveTo(target, event.modifiers)
            ensureCurrentVisible()
            prefix = ""
        } else if (event.key === Qt.Key_Space) {
            if (event.modifiers & Qt.ControlModifier) selection.toggle(selection.currentIndex)
            else selection.selectOnly(selection.currentIndex)
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

    function selectPrefix(text) {
        const entries = navigationController.entries
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
