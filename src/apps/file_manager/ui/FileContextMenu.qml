// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Shared right-click/keyboard context menu for both the Icon and Details
// views, so the two stay consistent instead of each hand-declaring its own
// item list. Every trigger dispatches through the existing AppShell action
// catalog/coordinator (root.appCoordinator.activateAction) — the same path
// the toolbar and menu bar already use — so this adds no new mutation policy
// or dispatch seam, only truthful, context-scoped visibility.
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

    // Reuses the exact enabled truth the menu bar/toolbar already read from
    // (coordinator.menus[].actions[].enabled, driven by
    // bindFileManagerTransferActions/bindFileManagerBrowsingActions from
    // mutation-busy/selection/clipboard state) instead of a second,
    // independently-computed policy that can drift out of sync with it (a
    // busy mutation must disable Cut/Copy/Paste/New Folder/Rename/Copy
    // To…/Move To…/Trash here exactly as it already does on the menu bar).
    // A fixture appCoordinator with no menus property (e.g.
    // tst_viewport.qml's minimal QtObject) falls back to true rather than
    // throwing on an undefined iteration.
    function actionEnabled(id) {
        const menuGroups = root.appCoordinator.menus
        if (!menuGroups)
            return true
        for (let i = 0; i < menuGroups.length; ++i) {
            const actions = menuGroups[i].actions
            if (!actions)
                continue
            for (let j = 0; j < actions.length; ++j) {
                if (actions[j].id === id)
                    return actions[j].enabled === true
            }
        }
        return true
    }

    // Background actions: apply to the browsed folder, not to any entry.
    MenuItem {
        objectName: "contextNewFolderAction"
        visible: root.isBackground && !root.applicationsPlace
        enabled: root.isBackground && root.actionEnabled("file.new-folder")
        text: qsTr("New Folder")
        onTriggered: root.appCoordinator.activateAction("file.new-folder")
    }
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
        // from Main.qml) needs no change to the frozen EntryGrid/EntryList
        // wiring that instantiates this menu.
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
    MenuItem {
        objectName: "contextRefreshAction"
        visible: root.isBackground
        enabled: root.isBackground
        text: qsTr("Refresh")
        onTriggered: root.appCoordinator.activateAction("view.refresh")
    }
    MenuItem {
        objectName: "contextViewModeAction"
        visible: root.isBackground
        enabled: root.isBackground
        text: root.navigationController.viewMode === "grid"
            ? qsTr("Details View") : qsTr("Icon View")
        onTriggered: root.appCoordinator.activateAction(
            root.navigationController.viewMode === "grid"
                ? "view.details-mode" : "view.grid-mode")
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

    // Selection actions: apply to the entries selectionCount describes.
    MenuItem {
        objectName: "contextOpenApplicationAction"
        visible: !root.isBackground && root.applicationsPlace
        enabled: visible && root.actionEnabled("application.open")
        text: qsTr("Open")
        onTriggered: root.appCoordinator.activateAction("application.open")
    }
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
    MenuItem {
        objectName: "contextRenameAction"
        visible: !root.isBackground && root.selectionCount === 1 && !root.applicationsPlace
        enabled: !root.isBackground && root.selectionCount === 1
            && root.actionEnabled("file.rename")
        text: qsTr("Rename")
        onTriggered: root.appCoordinator.activateAction("file.rename")
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
    MenuItem {
        objectName: "contextTrashAction"
        visible: !root.isBackground && !root.applicationsPlace
        enabled: !root.isBackground && root.actionEnabled("file.trash")
        text: qsTr("Move to Trash")
        onTriggered: root.appCoordinator.activateAction("file.trash")
    }
    MenuItem {
        objectName: "contextPropertiesAction"
        // Get Info describes one application at a time.
        visible: !root.isBackground && (!root.applicationsPlace || root.selectionCount === 1)
        enabled: visible && root.actionEnabled("file.properties")
        text: root.applicationsPlace ? qsTr("Get Info") : qsTr("Properties")
        onTriggered: root.appCoordinator.activateAction("file.properties")
    }
    MenuItem {
        objectName: "contextShowEntryFileAction"
        visible: !root.isBackground && root.applicationsPlace && root.selectionCount === 1
        enabled: visible && root.actionEnabled("application.show-entry-file")
        text: qsTr("Show Desktop Entry File")
        onTriggered: root.appCoordinator.activateAction("application.show-entry-file")
    }
}
