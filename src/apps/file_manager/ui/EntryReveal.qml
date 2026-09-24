// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// "Show in folder" for this window (ADR-0273): shows a folder with the named
// entries selected and scrolled into view, and opens their properties when
// asked. runtime/process_reveal_windows.cpp calls reveal() for
// org.freedesktop.FileManager1 requests and for the --select command line.
//
// AGENT-CONTRACT: C++ finds this object by its objectName "entryReveal" and
// calls reveal(folder, names, showProperties) through
// QMetaObject::invokeMethod with QVariant arguments. Keep the name, the
// untyped parameters and the boolean result (true once `folder` is shown).
QtObject {
    id: root

    required property var navigationController
    required property var selection
    required property var iconView
    required property var detailsView
    required property var propertiesController
    required property var infoDialog

    function reveal(folder, names, showProperties) {
        const navigation = root.navigationController
        let hidden = false
        for (const name of names)
            hidden = hidden || String(name).startsWith(".")
        // A hidden entry can be selected only while hidden entries are listed.
        if (hidden && !navigation.showHidden)
            navigation.setShowHidden(true)
        if (navigation.currentPath !== folder || !navigation.folderViewActive)
            navigation.navigateTo(folder)
        if (navigation.currentPath !== folder)
            return false
        const indexes = []
        for (const name of names) {
            const index = navigation.indexOfName(String(name))
            if (index >= 0)
                indexes.push(index)
        }
        if (indexes.length === 0)
            return true
        indexes.sort((left, right) => left - right)
        root.selection.applyIndexSet(indexes, Qt.NoModifier)
        // The same reveal ApplicationsPlaceActions.revealFile() performs.
        const view = navigation.viewMode === "grid" ? root.iconView : root.detailsView
        view.focusView()
        view.focusItem.forceLayout()
        view.focusItem.positionViewAtIndex(indexes[0], ListView.Contain)
        if (showProperties === true) {
            root.propertiesController.inspect(root.selection.selectedEntries())
            if (root.propertiesController.active)
                root.infoDialog.open()
        }
        return true
    }
}
