// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Shared right-click/keyboard context menu for both the Icon and Details
// views, so the two stay consistent instead of each hand-declaring its own
// item list. Every catalog item dispatches through the existing AppShell
// action catalog/coordinator (root.appCoordinator.activateAction) — the same
// path the toolbar and menu bar already use — so this adds no new mutation
// policy or dispatch seam, only truthful, context-scoped visibility. The
// right-click set's entries no static catalog action can name (one
// application of Open With ▸, one template of New File ▸, and the
// background's folder Get Info) call the window's FileActions (ADR-0269).
//
// selectionCount is set by the owning view immediately before popup(): 0
// means the background was targeted (empty space, or a keyboard invocation
// with no focused item); a positive count means that many existing entries
// are targeted. Rename is additionally restricted to exactly one entry
// because MutationDialogs.dispatch("file.rename") only ever acts on the
// first selected entry (mirrors the existing single-item Rename contract,
// not a new restriction invented here).
Menu {
    id: root

    required property var appCoordinator
    required property var navigationController
    // Optional: fixture tests may leave these null (items that need them are
    // then hidden rather than dispatching into a missing controller).
    property var clipboardController: null
    property var mutationController: null
    property var fileActions: null
    property int selectionCount: 0

    readonly property bool isBackground: selectionCount === 0
    readonly property bool canPaste: root.clipboardController
        ? root.clipboardController.canPaste : false
    // Compared rather than read directly so a minimal fixture navigation
    // object without a showHidden property (e.g. tst_viewport.qml) yields a
    // real false instead of an undefined-to-bool assignment warning.
    readonly property bool showHiddenChecked: root.navigationController.showHidden === true
    // ADR-0262: the Applications place offers application actions instead of
    // file ones; compared for the same minimal-fixture reason as above.
    readonly property bool applicationsPlace: root.navigationController.applicationsPlace === true
    // ADR-0269: the right-click set applies to local folders only.
    readonly property bool localFolder: !root.applicationsPlace
        && root.navigationController.remoteActive !== true && root.fileActions !== null
    readonly property bool inTrash: root.fileActions !== null && root.fileActions.inTrash === true
    readonly property bool itemActions: !root.isBackground && root.localFolder && !root.inTrash

    // What the targeted entries are, refreshed each time the menu opens
    // because the selection can change while selectionCount stays the same.
    property var selectionInfo: ({ "files": 0, "folders": 0, "archives": 0 })
    property var openWithCandidates: []
    readonly property bool onlyFolders: root.selectionInfo.folders > 0 && root.selectionInfo.files === 0

    // Reuses the exact enabled truth the menu bar/toolbar already read from
    // (coordinator.menus[].actions[].enabled, driven by the app_shell binders
    // from mutation-busy/selection/clipboard state) instead of a second,
    // independently-computed policy that can drift out of sync with it (a
    // busy mutation must disable Cut/Copy/Paste/New Folder/Rename/Copy
    // To…/Move To…/Trash here exactly as it already does on the menu bar).
    // A fixture appCoordinator with no menus property (e.g.
    // tst_viewport.qml's minimal QtObject) falls back to true rather than
    // throwing on an undefined iteration.
    function actionEnabled(id) {
        const action = root.catalogAction(id)
        return action === null || action.enabled === true
    }

    // The coordinator's published entry for `id`, or null.
    function catalogAction(id) {
        const menuGroups = root.appCoordinator.menus
        for (let i = 0; menuGroups && i < menuGroups.length; ++i) {
            const actions = menuGroups[i].actions || []
            for (let j = 0; j < actions.length; ++j) {
                if (actions[j].id === id)
                    return actions[j]
            }
        }
        return null
    }

    // QQC2 creates the entry that stands for a sub-menu itself, so its
    // visibility cannot be bound declaratively; it is set each time the menu
    // opens, and named (contextOpenWithMenu, contextNewFileMenu,
    // contextSortMenu, contextViewMenu) so tests can find it.
    function showSubMenu(menu, name, shown) {
        for (let i = 0; i < root.count; ++i) {
            const item = root.itemAt(i)
            if (item && item.subMenu === menu) {
                item.objectName = name
                item.visible = shown
                return
            }
        }
    }

    onAboutToShow: {
        root.selectionInfo = root.fileActions && !root.isBackground
            ? root.fileActions.describeSelection() : ({ "files": 0, "folders": 0, "archives": 0 })
        const openWith = !root.isBackground && root.localFolder
            && root.selectionInfo.files > 0 && root.selectionInfo.folders === 0
        root.openWithCandidates = openWith ? root.fileActions.openWithCandidates() : []
        const newFile = root.isBackground && root.localFolder && !root.inTrash
        if (newFile)
            root.fileActions.refreshTemplates()
        root.showSubMenu(openWithMenu, "contextOpenWithMenu", openWith)
        root.showSubMenu(newFileMenu, "contextNewFileMenu", newFile)
        root.showSubMenu(sortMenu, "contextSortMenu", root.isBackground)
        root.showSubMenu(viewMenu, "contextViewMenu", root.isBackground)
    }

    // The same menus' checked truth, for checkable actions set in C++.
    function actionChecked(id) {
        const action = root.catalogAction(id)
        return action !== null && action.checked === true
    }

    // Background actions: apply to the browsed folder, not to any entry.
    MenuItem {
        objectName: "contextNewFolderAction"
        visible: root.isBackground && !root.applicationsPlace
        enabled: root.isBackground && root.actionEnabled("file.new-folder")
        text: qsTr("New Folder")
        onTriggered: root.appCoordinator.activateAction("file.new-folder")
    }
    ContextNewFileMenu { id: newFileMenu; contextMenu: root }
    MenuItem {
        id: backgroundPasteItem
        objectName: "contextBackgroundPasteAction"
        visible: root.isBackground && root.canPaste && !root.applicationsPlace
        enabled: root.isBackground && root.actionEnabled("edit.paste")
        text: qsTr("Paste")
        // Still activates the same "edit.paste" action every other Paste
        // trigger uses, so it crosses AppShell's known/enabled consent gate
        // exactly like the toolbar and menu bar do. Main.qml's
        // pasteDestination() resolves the focused entry when one is a
        // directory (so ordinary focused-entry paste keeps working), which is
        // wrong here — a background invocation must always land in the
        // folder being browsed even while an unrelated directory elsewhere
        // remains focused. backgroundPasteOverride is the narrow, self-
        // clearing seam Main.qml exposes for exactly that one case; reaching
        // it through Window.window (rather than a new property threaded down
        // from Main.qml) needs no change to the EntryGrid/EntryList wiring.
        onTriggered: {
            const window = backgroundPasteItem.Window.window
            if (window)
                window.backgroundPasteOverride = root.navigationController.currentPath
            root.appCoordinator.activateAction("edit.paste")
            if (window)
                window.backgroundPasteOverride = ""
        }
    }
    MenuSeparator { visible: root.isBackground }
    ContextActionItem {
        objectName: "contextOpenTerminalAction"; contextMenu: root; actionId: "file.open-terminal"
        visible: root.isBackground && root.localFolder; text: qsTr("Open Terminal Here")
    }
    ContextActionItem {
        objectName: "contextSelectAllAction"; contextMenu: root; actionId: "edit.select-all"
        visible: root.isBackground; text: qsTr("Select All")
    }
    ContextChoiceMenu {
        id: sortMenu
        objectName: "contextSortSubMenu"
        title: qsTr("Sort By")
        contextMenu: root
        current: root.navigationController.sortColumn
        choices: [
            { name: "contextSortNameAction", text: qsTr("Name"), actionId: "view.sort-name", value: "name" },
            { name: "contextSortSizeAction", text: qsTr("Size"), actionId: "view.sort-size", value: "size" },
            { name: "contextSortKindAction", text: qsTr("Kind"), actionId: "view.sort-kind", value: "kind" },
            { name: "contextSortModifiedAction", text: qsTr("Date Modified"), actionId: "view.sort-modified", value: "modified" }
        ]
    }
    ContextChoiceMenu {
        id: viewMenu
        objectName: "contextViewSubMenu"
        title: qsTr("View")
        contextMenu: root
        current: root.navigationController.viewMode
        choices: [
            { name: "contextIconViewAction", text: qsTr("Icon View"), actionId: "view.grid-mode", value: "grid" },
            { name: "contextDetailsViewAction", text: qsTr("Details View"), actionId: "view.details-mode", value: "list" }
        ]
    }
    MenuItem {
        objectName: "contextShowHiddenAction"
        visible: root.isBackground && !root.applicationsPlace
        enabled: root.isBackground
        checkable: true
        checked: root.showHiddenChecked
        text: qsTr("Show Hidden Files")
        onTriggered: root.appCoordinator.activateAction("view.show-hidden")
    }
    MenuItem {
        objectName: "contextGroupByCategoryAction"
        visible: root.isBackground && root.applicationsPlace
        enabled: visible && root.actionEnabled("view.group-by-category")
        checkable: true
        checked: root.navigationController.sortColumn === "kind"
        text: qsTr("Group by Category")
        onTriggered: root.appCoordinator.activateAction("view.group-by-category")
    }
    MenuItem {
        objectName: "contextRefreshAction"
        visible: root.isBackground
        enabled: root.isBackground
        text: qsTr("Refresh")
        onTriggered: root.appCoordinator.activateAction("view.refresh")
    }
    MenuItem {
        objectName: "contextFolderInfoAction"
        visible: root.isBackground && root.localFolder
        enabled: root.actionEnabled("file.properties")
        text: qsTr("Get Info")
        onTriggered: root.fileActions.showFolderInfo()
    }

    // Selection actions: apply to the entries selectionCount describes.
    // ADR-0269: one Open for files, folders and application rows alike.
    ContextActionItem {
        objectName: "contextOpenAction"; contextMenu: root; actionId: "file.open"
        visible: !root.isBackground; text: qsTr("Open")
    }
    ContextOpenWithMenu { id: openWithMenu; contextMenu: root }
    ContextActionItem {
        objectName: "contextOpenNewWindowAction"; contextMenu: root; actionId: "file.open-new-window"
        visible: !root.isBackground && root.localFolder && root.onlyFolders
        text: qsTr("Open in New Window")
    }
    MenuSeparator { visible: !root.isBackground && !root.applicationsPlace }
    MenuItem {
        objectName: "contextCutAction"
        visible: !root.isBackground && !root.applicationsPlace
        enabled: !root.isBackground && root.actionEnabled("edit.cut")
        text: qsTr("Cut")
        onTriggered: root.appCoordinator.activateAction("edit.cut")
    }
    MenuItem {
        objectName: "contextClipboardCopyAction"
        visible: !root.isBackground && !root.applicationsPlace
        enabled: !root.isBackground && root.actionEnabled("edit.copy")
        text: qsTr("Copy")
        onTriggered: root.appCoordinator.activateAction("edit.copy")
    }
    ContextActionItem {
        objectName: "contextCopyPathAction"; contextMenu: root; actionId: "edit.copy-path"
        visible: !root.isBackground && root.localFolder; text: qsTr("Copy Path")
    }
    MenuItem {
        objectName: "contextRenameAction"
        visible: !root.isBackground && root.selectionCount === 1 && !root.applicationsPlace
        enabled: !root.isBackground && root.selectionCount === 1
            && root.actionEnabled("file.rename")
        text: qsTr("Rename")
        onTriggered: root.appCoordinator.activateAction("file.rename")
    }
    ContextActionItem {
        objectName: "contextDuplicateAction"; contextMenu: root; actionId: "file.duplicate"
        visible: root.itemActions; text: qsTr("Duplicate")
    }
    ContextActionItem {
        objectName: "contextMakeLinkAction"; contextMenu: root; actionId: "file.make-link"
        visible: root.itemActions; text: qsTr("Make Link")
    }
    MenuItem {
        objectName: "contextCopyAction"
        visible: !root.isBackground && !root.applicationsPlace
        enabled: !root.isBackground && root.actionEnabled("file.copy")
        text: qsTr("Copy To…")
        onTriggered: root.appCoordinator.activateAction("file.copy")
    }
    MenuItem {
        objectName: "contextMoveAction"
        visible: !root.isBackground && !root.applicationsPlace
        enabled: !root.isBackground && root.actionEnabled("file.move")
        text: qsTr("Move To…")
        onTriggered: root.appCoordinator.activateAction("file.move")
    }
    MenuSeparator { visible: root.itemActions }
    ContextActionItem {
        objectName: "contextCompressAction"; contextMenu: root; actionId: "file.compress"
        visible: root.itemActions
        text: root.selectionCount === 1 ? qsTr("Compress") : qsTr("Compress %1 Items").arg(root.selectionCount)
    }
    ContextActionItem {
        objectName: "contextExtractAction"; contextMenu: root; actionId: "file.extract"
        visible: root.itemActions && root.selectionInfo.archives > 0 && root.selectionInfo.archives === root.selectionCount
        text: qsTr("Extract")
    }
    ContextActionItem {
        objectName: "contextAddToSidebarAction"; contextMenu: root; actionId: "file.add-to-sidebar"
        visible: root.itemActions && root.onlyFolders; text: qsTr("Add to Sidebar")
    }
    MenuSeparator { visible: !root.isBackground }
    MenuItem {
        objectName: "contextPropertiesAction"
        // Get Info describes one application at a time.
        visible: !root.isBackground && (!root.applicationsPlace || root.selectionCount === 1)
        enabled: visible && root.actionEnabled("file.properties")
        text: qsTr("Get Info")
        onTriggered: root.appCoordinator.activateAction("file.properties")
    }
    MenuItem {
        objectName: "contextShowEntryFileAction"
        visible: !root.isBackground && root.applicationsPlace && root.selectionCount === 1
        enabled: visible && root.actionEnabled("application.show-entry-file")
        text: qsTr("Show Desktop Entry File")
        onTriggered: root.appCoordinator.activateAction("application.show-entry-file")
    }
    MenuItem {
        // ADR-0273: checked only once the dock confirms the application.
        objectName: "contextKeepInDockAction"
        visible: !root.isBackground && root.applicationsPlace && root.selectionCount === 1
        enabled: visible && root.actionEnabled("application.keep-in-dock")
        checkable: true
        checked: root.actionChecked("application.keep-in-dock")
        text: qsTr("Keep in Dock")
        onTriggered: root.appCoordinator.activateAction("application.keep-in-dock")
    }
    ContextActionItem {
        objectName: "contextPutBackAction"; contextMenu: root; actionId: "file.put-back"
        visible: !root.isBackground && root.inTrash; text: qsTr("Put Back")
    }
    MenuItem {
        objectName: "contextTrashAction"
        visible: !root.isBackground && !root.applicationsPlace && !root.inTrash
        enabled: !root.isBackground && root.actionEnabled("file.trash")
        text: qsTr("Move to Trash")
        onTriggered: root.appCoordinator.activateAction("file.trash")
    }
    ContextActionItem {
        objectName: "contextDeleteAction"; contextMenu: root; actionId: "file.delete"
        visible: !root.isBackground && root.localFolder; text: qsTr("Delete Permanently")
    }
}
