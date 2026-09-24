// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The Applications place's window actions (ADR-0262), kept out of Main.qml:
// Open, Get Info (file.properties there), Show Desktop Entry File and Group
// by Category. Availability is decided in C++ (file_manager_application_actions);
// this item only carries each action out and owns the Get Info dialog.
Item {
    id: root

    required property var applicationsController
    required property var navigationController
    // The window's EntrySelection and its two views (Main.qml's instances).
    required property var selection
    required property var iconView
    required property var detailsView

    function activeView() {
        return root.navigationController.viewMode === "grid" ? root.iconView : root.detailsView
    }

    // Returns true when actionId belongs to the Applications place and was
    // handled here; every other action stays with Main.qml's dispatch.
    function handle(actionId) {
        const navigation = root.navigationController
        if (navigation.applicationsPlace !== true)
            return false
        const entries = root.selection.selectedEntries()
        if (actionId === "application.open") {
            // Each row opens through NavigationController.activate(), the same
            // path as a double-click, so its ApplicationsFileLauncher seam (and
            // chooser mode, ADR-0165) apply to every trigger.
            for (const entry of entries)
                navigation.activate(root.selection.indexOfKey(root.selection.key(entry)))
            return true
        }
        if (actionId === "file.properties") {
            if (entries.length > 0)
                infoDialog.show(root.applicationsController.describe(entries[0].applicationId))
            return true
        }
        if (actionId === "application.show-entry-file") {
            if (entries.length === 1)
                root.revealFile(root.applicationsController.describe(
                    entries[0].applicationId).desktopFilePath || "")
            return true
        }
        if (actionId === "view.group-by-category") {
            // Grouping is the category sort plus the views' section breaks.
            navigation.setSortColumn(navigation.sortColumn === "kind" ? "name" : "kind")
            return true
        }
        return false
    }

    // Opens the folder holding path and selects and reveals it there.
    function revealFile(path) {
        const slash = path.lastIndexOf("/")
        if (slash < 0)
            return
        root.navigationController.navigateTo(slash === 0 ? "/" : path.substring(0, slash))
        const index = root.navigationController.indexOfName(path.substring(slash + 1))
        if (index < 0)
            return
        root.selection.selectOnly(index)
        const view = root.activeView()
        view.focusView()
        view.focusItem.forceLayout()
        view.focusItem.positionViewAtIndex(index, ListView.Contain)
    }

    ApplicationInfoDialog {
        id: infoDialog
    }
}
