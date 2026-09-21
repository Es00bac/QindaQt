// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaTK.QindaQt
import QindaQt.QindaLutris
import "parts" as Parts

// The window: one LibraryPage fed by the Library singleton, the menu from
// the same AppShell catalog the desktop may host, and the status line the
// controller's degradation notes land on. All data reaches the page as
// properties, so the page stays constructible over a stub in tests.
Tk.AppWindow {
    id: window

    // AGENT-CONTRACT: written by the AppShell menu export in main.cpp. True
    // until the desktop's global menu takes the menu over, false once it
    // hosts it. Nothing else may assign it.
    property bool inWindowMenuVisible: true
    property string message: ""

    width: 1180
    height: 760
    visible: true
    title: qsTr("QindaLutris")

    // Feeds the desktop's QST-1 tokens into the toolkit theme.
    QindaQtTheme {}

    readonly property var actionList: {
        let actions = []
        for (const menu of coordinator.menus) {
            actions = actions.concat(menu.actions)
        }
        return actions
    }

    function dispatch(actionId) {
        if (actionId === "file.add-windows-game") {
            window.openAddWine()
            return
        }
        if (actionId === "file.refresh") {
            Library.refresh()
            return
        }
        if (actionId === "file.quit") {
            Qt.quit()
            return
        }
        if (actionId === "help.about") {
            about.open()
        }
    }

    Connections {
        target: coordinator
        function onActionRequested(actionId) { window.dispatch(actionId) }
    }
    Connections {
        target: Library
        function onLaunchFailed(message) { window.flash(message) }
        function onStoreError(message) { window.flash(message) }
        function onStatusMessageChanged() {
            if (Library.statusMessage.length > 0) {
                window.flash(Library.statusMessage)
            }
        }
        function onSelectedGameChanged() { page.pullOptions() }
    }

    function flash(text) {
        window.message = text
        messageTimer.restart()
    }

    function openAddWine() {
        // Proton discovery lands at refresh time; pull the choices as the
        // dialog opens rather than binding once at construction.
        addWine.protonChoices = Library.protonChoices()
        addWine.open()
    }

    Instantiator {
        model: window.actionList
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.shortcut
            enabled: modelData.enabled
            onActivated: coordinator.activateAction(modelData.id)
        }
    }

    menuBar: Parts.AppMenuBar {
        objectName: "appMenuBar"
        visible: window.inWindowMenuVisible
        menusModel: coordinator.menus
        onActivated: actionId => coordinator.activateAction(actionId)
    }

    LibraryPage {
        id: page
        objectName: "libraryPage"
        anchors.fill: parent
        gameModel: Library.gameModel
        totalCount: Library.totalCount
        shownCount: Library.gameModel.shownCount
        availableSources: Library.sourcesPresent
        displays: Library.displays
        selectedGame: Library.selectedGame
        selectedPlayable: Library.selectedPlayable
        selectedPlayReason: Library.selectedPlayReason

        onSelectRequested: function(gameId) { Library.selectGame(gameId) }
        onPlayRequested: { Library.playSelected() }
        onRefreshRequested: { Library.refresh() }
        onSearchChanged: function(text) { Library.gameModel.searchText = text }
        onSourceFilterChanged: function(sourceId) { Library.gameModel.sourceFilter = sourceId }
        onOptionsSaveRequested: function(values) { Library.saveLaunchOptionsForSelected(values) }
        onRemoveWineRequested: function(gameId) { Library.removeWineGame(gameId) }
        onAddWineRequested: { window.openAddWine() }

        function pullOptions() {
            page.selectedOptions = Library.launchOptionsForSelected()
        }
        Component.onCompleted: pullOptions()
    }

    Parts.AddWineDialog {
        id: addWine
        onAddRequested: function(values) {
            if (!Library.addWineGame(values.title, values.executable,
                                     values.prefix, values.runner,
                                     values.proton)) {
                window.flash(qsTr("That game could not be added — check the title and the executable path"))
            }
        }
    }

    statusBar: Tk.StatusBar {
        Tk.StatusField {
            objectName: "statusCount"
            iconName: "gamepad-2"
            text: qsTr("%1 games").arg(Library.totalCount)
        }
        Tk.Spacer {}
        Tk.StatusField {
            objectName: "statusMessage"
            iconName: window.message.length > 0 ? "triangle-alert" : ""
            text: window.message
        }
    }

    Timer {
        id: messageTimer
        interval: 8000
        onTriggered: window.message = ""
    }

    Tk.MessageDialog {
        id: about
        title: qsTr("QindaLutris")
        text: qsTr("QindaLutris — the QindaQt game library.\n\n"
                   + "Steam, Lutris, native Linux games and your own Windows "
                   + "games in one window. Launch options, gamemode, MangoHud "
                   + "and a target display are kept per game.")
    }
}
