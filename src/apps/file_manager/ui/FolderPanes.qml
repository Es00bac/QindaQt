// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The window's folder area (ADR-0271): one pane, or two side by side in the
// Commander style, each with its own tabs, and every tab browsing with its
// own NavigationController. It owns the tabs' lifetime, which pane and tab
// the user works in, the tab keys (Ctrl+T, Ctrl+W, Ctrl+Tab, Ctrl+Shift+Tab)
// and Commander's keys (Tab between the panes, F3 to F8).
//
// AGENT-CONTRACT: Main.qml reaches the tab the user works in only through
// activeTab, activeNavigation, activeSelection and activeViews. The first
// tab holds the window's injected NavigationController (main.cpp) and is
// never destroyed -- closing it only hides it -- so none of these is ever
// null. Every other controller comes from `navigationFactory`
// (runtime/folder_navigations.h), which this object tells which controller
// is active; without a factory (fixture windows) the window has exactly one
// tab and one pane, as before ADR-0271.
RowLayout {
    id: root
    objectName: "folderPanes"

    required property var navigationController
    property var navigationFactory: null
    required property var appCoordinator
    property var mutationController: null
    property var clipboardController: null
    property var fileActions: null
    required property var preferencesController
    property var entryFacts: null
    property var columnListing: null
    // The Commander style: two panes, Tab between them, the function keys.
    property bool dualPane: false

    property int activePaneIndex: 0
    readonly property bool twoPanes: root.dualPane && root.navigationFactory !== null
    readonly property var activePane: root.twoPanes && root.activePaneIndex === 1
        ? rightPane : leftPane
    readonly property var otherPane: root.twoPanes
        ? (root.activePaneIndex === 1 ? leftPane : rightPane) : null
    readonly property var activeTab: root.activePane.currentTab !== null
        ? root.activePane.currentTab : firstTab
    readonly property var activeNavigation: root.activeTab.navigationController
    readonly property var activeSelection: root.activeTab.selection
    readonly property var activeViews: root.activeTab.views
    // Commander's Copy To and Move To start in the other pane's folder.
    readonly property string otherPanePath: root.otherPane !== null && root.otherPane.navigation !== null
        ? root.otherPane.navigation.currentPath : ""
    // AGENT-CONTRACT: Commander's function keys, in the bar's order. Each runs
    // its catalog action through the coordinator, exactly as its menu item
    // does; F5 and F6 open Copy To / Move To at the other pane
    // (MutationDialogs.transferTarget); F3 is Quick Look (ADR-0272).
    readonly property var functionKeys: [
        { "key": Qt.Key_F3, "label": "F3", "text": qsTr("View"), "action": "file.quick-look" },
        { "key": Qt.Key_F4, "label": "F4", "text": qsTr("Edit"), "action": "file.open-with" },
        { "key": Qt.Key_F5, "label": "F5", "text": qsTr("Copy"), "action": "file.copy" },
        { "key": Qt.Key_F6, "label": "F6", "text": qsTr("Move"), "action": "file.move" },
        { "key": Qt.Key_F7, "label": "F7", "text": qsTr("New Folder"), "action": "file.new-folder" },
        { "key": Qt.Key_F8, "label": "F8", "text": qsTr("Delete"), "action": "file.trash" }
    ]

    spacing: 0

    onActiveNavigationChanged: {
        if (root.navigationFactory)
            root.navigationFactory.setActive(root.activeNavigation)
    }
    onTwoPanesChanged: root.settlePanes()
    Component.onCompleted: root.settlePanes()

    function focusActiveView() {
        Qt.callLater(() => root.activeViews.activeView.focusView())
    }

    // Commander's second pane starts in the folder the first one shows.
    function settlePanes() {
        if (root.twoPanes && rightPane.tabs.length === 0)
            root.openTab(rightPane, root.activeNavigation.currentPath)
        if (!root.twoPanes)
            root.activePaneIndex = 0
    }

    // A new tab in `pane` browsing `path`; null without a factory.
    function openTab(pane, path) {
        if (!root.navigationFactory)
            return null
        const navigation = root.navigationFactory.create(path)
        if (!navigation)
            return null
        const tab = tabComponent.createObject(pane.tabArea, {
            "pane": pane, "navigationController": navigation })
        pane.tabs = pane.tabs.concat([tab])
        pane.currentIndex = pane.tabs.length - 1
        return tab
    }

    // Ctrl+T: a new tab at the folder the user is in, as Finder does.
    function newTab() {
        if (root.openTab(root.activePane, root.activeNavigation.currentPath))
            root.focusActiveView()
    }

    function closeTab(pane, index) {
        const tabs = pane.tabs
        if (index < 0 || index >= tabs.length)
            return
        if (tabs.length === 1) {
            // The last tab of the only pane closes the window, as a Finder
            // window's does; a Commander pane keeps its last tab.
            if (!root.twoPanes)
                root.Window.window.close()
            return
        }
        const tab = tabs[index]
        const current = pane.currentIndex > index ? pane.currentIndex - 1 : pane.currentIndex
        // AGENT-GUARD: the pane forgets the tab first, which moves the
        // active controller (onActiveNavigationChanged) before the views bound
        // to the closed one go and, last, the controller itself.
        pane.tabs = tabs.slice(0, index).concat(tabs.slice(index + 1))
        pane.currentIndex = Math.min(current, pane.tabs.length - 1)
        tab.pane = null
        root.focusActiveView()
        if (tab === firstTab)
            return // the window's own controller and its tab stay, hidden
        const navigation = tab.navigationController
        tab.destroy()
        root.navigationFactory.release(navigation)
    }

    function cycleTab(step) {
        const pane = root.activePane
        const count = pane.tabs.length
        if (count < 2)
            return
        pane.currentIndex = (pane.currentIndex + step + count) % count
        root.focusActiveView()
    }

    function switchPane() {
        root.activePaneIndex = root.activePaneIndex === 1 ? 0 : 1
        root.focusActiveView()
    }

    // The folder shown beside the one a file operation just changed may be
    // its destination (Commander's F5/F6), so it reads its folder again too.
    function refreshOtherPane() {
        if (root.otherPane !== null && root.otherPane.navigation !== null)
            root.otherPane.navigation.refresh()
    }

    // The Commander key a press means, or "" (Tab, or a function key's action).
    function commanderCommand(event) {
        const modifiers = Qt.ControlModifier | Qt.AltModifier | Qt.ShiftModifier | Qt.MetaModifier
        if (!root.twoPanes || (event.modifiers & modifiers))
            return ""
        if (event.key === Qt.Key_Tab)
            return "pane.switch"
        const entry = root.functionKeys.find(candidate => candidate.key === event.key)
        return entry ? entry.action : ""
    }

    function runCommand(command) {
        if (command === "pane.switch")
            root.switchPane()
        else
            root.appCoordinator.activateAction(command)
    }

    // AGENT-NOTE: F5 is also Refresh in the shared action catalog. Claiming
    // the Commander keys at ShortcutOverride, while focus is in a pane, is
    // what lets them win there; Ctrl+R still refreshes.
    Keys.onShortcutOverride: (event) => event.accepted = root.commanderCommand(event) !== ""
    Keys.onPressed: (event) => {
        const command = root.commanderCommand(event)
        if (command === "")
            return
        event.accepted = true
        root.runCommand(command)
    }

    Shortcut {
        sequence: "Ctrl+T"
        enabled: root.navigationFactory !== null
        onActivated: root.newTab()
    }
    Shortcut {
        sequence: "Ctrl+W"
        enabled: root.navigationFactory !== null
        onActivated: root.closeTab(root.activePane, root.activePane.currentIndex)
    }
    Shortcut {
        sequence: "Ctrl+Tab"
        enabled: root.navigationFactory !== null
        onActivated: root.cycleTab(1)
    }
    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        enabled: root.navigationFactory !== null
        onActivated: root.cycleTab(-1)
    }

    // Focus entering a pane's views makes it the active pane, whether by a
    // click, Tab or the header.
    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            if (!root.twoPanes)
                return
            let item = target.activeFocusItem
            while (item && item !== leftPane && item !== rightPane)
                item = item.parent
            if (item === leftPane)
                root.activePaneIndex = 0
            else if (item === rightPane)
                root.activePaneIndex = 1
        }
    }

    Component {
        id: tabComponent
        FolderTab {
            appCoordinator: root.appCoordinator
            mutationController: root.mutationController
            clipboardController: root.clipboardController
            fileActions: root.fileActions
            preferencesController: root.preferencesController
            entryFacts: root.entryFacts
            columnListing: root.columnListing
        }
    }

    FolderPane {
        id: leftPane
        objectName: "leftPane"
        Layout.fillWidth: true
        Layout.fillHeight: true
        tabs: [firstTab]
        active: root.activePane === leftPane
        showHeader: root.twoPanes
        paneName: root.twoPanes ? qsTr("Left pane") : qsTr("Folder")
        mutationController: root.mutationController
        clipboardController: root.clipboardController
        onActivated: { root.activePaneIndex = 0; root.focusActiveView() }
        onNewTabRequested: { root.activePaneIndex = 0; root.newTab() }
        onCloseTabRequested: (index) => root.closeTab(leftPane, index)

        // The window's first tab, over the injected controller (see the
        // contract above).
        FolderTab {
            id: firstTab
            pane: leftPane
            navigationController: root.navigationController
            appCoordinator: root.appCoordinator
            mutationController: root.mutationController
            clipboardController: root.clipboardController
            fileActions: root.fileActions
            preferencesController: root.preferencesController
            entryFacts: root.entryFacts
            columnListing: root.columnListing
        }
    }

    ToolSeparator {
        Layout.fillHeight: true
        orientation: Qt.Vertical
        padding: 0
        visible: root.twoPanes
    }

    FolderPane {
        id: rightPane
        objectName: "rightPane"
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.twoPanes
        active: root.activePane === rightPane
        showHeader: true
        paneName: qsTr("Right pane")
        mutationController: root.mutationController
        clipboardController: root.clipboardController
        onActivated: { root.activePaneIndex = 1; root.focusActiveView() }
        onNewTabRequested: { root.activePaneIndex = 1; root.newTab() }
        onCloseTabRequested: (index) => root.closeTab(rightPane, index)
    }
}
