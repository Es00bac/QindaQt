// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// "Show in folder" for this window (ADR-0273): shows a folder with the named
// entries selected and scrolled into view, then runs one File Manager action
// on that selection through the coordinator, exactly as its menu item would
// (Get Info, Open With, New File or Quick Look).
// runtime/process_reveal_windows.cpp calls reveal() for
// org.freedesktop.FileManager1 requests and for the --select/--action command
// line.
//
// AGENT-CONTRACT: C++ finds this object by its objectName "entryReveal" and
// calls reveal(folder, names, action) through QMetaObject::invokeMethod with
// QVariant arguments. Keep the name, the untyped parameters and the boolean
// result (true once `folder` is shown).
QtObject {
    id: root

    required property var navigationController
    required property var selection
    // The window's FolderViewStack; the entries are revealed in its active view.
    required property var views
    required property var coordinator

    function reveal(folder, names, action) {
        const navigation = root.navigationController
        let hidden = false
        for (const name of names)
            hidden = hidden || String(name).startsWith(".")
        // A hidden entry can be selected only while hidden entries are listed.
        if (hidden && !navigation.showHidden)
            navigation.setShowHidden(true)
        if (navigation.currentPath !== folder || !navigation.folderViewActive)
            navigation.navigateTo(folder)
        else if (names.length > 0)
            navigation.refresh() // an entry saved since the last listing, a download above all
        if (navigation.currentPath !== folder)
            return false
        const indexes = []
        for (const name of names) {
            const index = navigation.indexOfName(String(name))
            if (index >= 0)
                indexes.push(index)
        }
        if (indexes.length > 0) {
            indexes.sort((left, right) => left - right)
            root.selection.applyIndexSet(indexes, Qt.NoModifier)
            // The same reveal ApplicationsPlaceActions.revealFile() performs.
            const view = root.views.activeView
            view.focusView()
            view.revealIndex(indexes[0])
        }
        // The action's own enabled state decides, as for its menu item: Open
        // With needs a selected file, Get Info describes the selection or,
        // with nothing selected, the folder. When none of the named entries
        // is there any more, the action would describe something else: skip.
        if (String(action).length > 0 && (names.length === 0 || indexes.length > 0))
            root.coordinator.activateAction(String(action))
        return true
    }
}
