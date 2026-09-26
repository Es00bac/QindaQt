// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import "parts" as Parts

// The one screen of QindaLutris: search and source chips over a cover grid,
// with the selected game's detail and launch options beside it. Every value
// arrives as a property and every action leaves as a signal, so this page
// constructs over a stub model in tests without the Library singleton.
Item {
    id: page

    // The filtered model (C++ GameFilterModel in the app, any role-compatible
    // model in tests). Roles: gameId, title, sourceId, sourceLabel, coverUrl.
    required property var gameModel
    property int totalCount: 0
    property int shownCount: 0
    property var availableSources: []
    property var displays: []
    property var selectedGame: ({})
    property var selectedOptions: ({})
    property bool selectedPlayable: false
    property string selectedPlayReason: ""
    property bool selectedRunning: false
    property var selectedVerdict: ({})

    property string currentSourceFilter: ""

    signal selectRequested(string gameId)
    signal playRequested()
    signal refreshRequested()
    signal searchChanged(string text)
    signal sourceFilterChanged(string sourceId)
    signal optionsSaveRequested(var values)
    signal removeWineRequested(string gameId)
    signal addWineRequested()
    signal forceQuitRequested(string gameId)
    signal confirmVersionRequested()

    readonly property bool hasSelection: selectedGame !== null
                                         && selectedGame.id !== undefined
    readonly property bool libraryEmpty: totalCount === 0

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        padding: Tk.Theme.space.sm

        // Search + chips + actions.
        Tk.Flex {
            direction: Tk.Flex.Row
            align: Tk.Flex.Center
            gap: Tk.Theme.space.sm
            Tk.Flex.alignSelf: Tk.Flex.Stretch

            Tk.SearchField {
                objectName: "librarySearch"
                Tk.Flex.grow: 1
                Tk.Flex.basis: 0
                onSearched: function(text) { page.searchChanged(text) }
            }
            Tk.Chip {
                objectName: "chipAll"
                text: qsTr("All")
                checkable: true
                checked: page.currentSourceFilter === ""
                onClicked: {
                    page.currentSourceFilter = ""
                    page.sourceFilterChanged("")
                }
            }
            Repeater {
                model: page.availableSources
                delegate: Tk.Chip {
                    required property var modelData
                    objectName: "chip-" + modelData
                    text: modelData === "steam" ? qsTr("Steam")
                        : modelData === "lutris" ? qsTr("Lutris")
                        : modelData === "desktop" ? qsTr("Native")
                        : modelData === "installed" ? qsTr("Installed")
                        : qsTr("Wine")
                    checkable: true
                    checked: page.currentSourceFilter === modelData
                    onClicked: {
                        page.currentSourceFilter = modelData
                        page.sourceFilterChanged(modelData)
                    }
                }
            }
            Tk.Chip {
                objectName: "addWineChip"
                text: qsTr("Add Windows game")
                iconName: "plus"
                tooltip: qsTr("Add a Windows executable run through Wine or Proton")
                onClicked: page.addWineRequested()
            }
        }

        // Grid + detail.
        Tk.Flex {
            direction: Tk.Flex.Row
            gap: Tk.Theme.space.sm
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Tk.Flex.alignSelf: Tk.Flex.Stretch

            Item {
                Tk.Flex.grow: 1
                Tk.Flex.basis: 0
                Tk.Flex.alignSelf: Tk.Flex.Stretch

                Parts.CoverGrid {
                    id: grid
                    anchors.fill: parent
                    visible: page.shownCount > 0
                    model: page.gameModel
                    selectedGameId: page.hasSelection ? page.selectedGame.id : ""
                    onSelectRequested: function(gameId) { page.selectRequested(gameId) }
                    onPlayRequested: { page.playRequested() }
                }
                Tk.EmptyState {
                    anchors.fill: parent
                    visible: page.shownCount === 0
                    iconName: "gamepad-2"
                    title: page.libraryEmpty ? qsTr("No games yet")
                                           : qsTr("Nothing matches")
                    text: page.libraryEmpty
                          ? qsTr("Games from Steam, Lutris and your desktop appear "
                                 + "here once installed. You can also add a Windows "
                                 + "game yourself.")
                          : qsTr("Try another search or source.")
                    Tk.Button {
                        visible: page.libraryEmpty
                        text: qsTr("Add Windows game")
                        variant: "accent"
                        onClicked: page.addWineRequested()
                    }
                }
            }

            Parts.GameDetail {
                id: detail
                Tk.Flex.shrink: 0
                width: 320
                visible: page.hasSelection
                game: page.selectedGame
                options: page.selectedOptions
                displays: page.displays
                playable: page.selectedPlayable
                playReason: page.selectedPlayReason
                running: page.selectedRunning
                verdict: page.selectedVerdict
                onPlayRequested: { page.playRequested() }
                onForceQuitRequested: function(gameId) { page.forceQuitRequested(gameId) }
                onConfirmVersionRequested: { page.confirmVersionRequested() }
                onOptionsSaveRequested: function(values) { page.optionsSaveRequested(values) }
                onRemoveRequested: function(gameId) { page.removeWineRequested(gameId) }
            }
        }
    }
}
