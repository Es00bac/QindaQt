// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaTK as Tk
import "../../../../src/apps/qindalutris/qml" as App

// The library page over a stub model (ADR-0231): it must construct, parent
// into the window, size itself positively, render one tile per stub row, and
// show an honest empty state when there is nothing to show.
Item {
    id: root
    width: 1180
    height: 760

    ListModel {
        id: stubGames
        ListElement { gameId: "steam/1"; title: "Fixture Quest"; sourceId: "steam"; sourceLabel: "Steam"; coverUrl: "" }
        ListElement { gameId: "desktop/tux"; title: "Tux Fixture"; sourceId: "desktop"; sourceLabel: "Native"; coverUrl: "" }
    }

    App.LibraryPage {
        id: page
        anchors.fill: parent
        gameModel: stubGames
        totalCount: 2
        shownCount: 2
        availableSources: ["steam", "desktop"]
        displays: [{ key: "DP-1", label: "DP-1" }]
        selectedGame: ({})
    }

    function named(item, name) {
        let result = item.objectName === name ? [item] : []
        for (let child of item.children || [])
            result = result.concat(named(child, name))
        return result
    }

    TestCase {
        name: "QindaLutrisLibraryPage"
        when: windowShown

        function init() {
            page.totalCount = 2
            page.shownCount = 2
            page.selectedGame = ({})
            wait(30)
        }

        function test_constructsWithWindowParentChain() {
            let node = page
            const chain = []
            while (node) {
                chain.push(node)
                node = node.parent
            }
            verify(chain.indexOf(root) >= 0,
                   "page parents up to the test root")
            verify(page.Window.window !== null, "page is inside a window")
            verify(page.width > 0 && page.height > 0,
                   "page has a positive size: " + page.width + "x" + page.height)
        }

        function test_oneTilePerStubRow() {
            wait(30)
            compare(named(page, "cover-steam/1").length, 1)
            compare(named(page, "cover-desktop/tux").length, 1)
        }

        function test_emptyLibraryIsAnHonestEmptyState() {
            page.totalCount = 0
            page.shownCount = 0
            wait(30)
            const titles = named(page, "emptyTitle")
            compare(titles.length, 1)
            verify(titles[0].visible)
        }

        function test_detailShowsSelectionAndPlayState() {
            page.selectedGame = ({
                id: "steam/1", title: "Fixture Quest", sourceId: "steam",
                sourceLabel: "Steam", installPath: "/fixture/steamapps/common",
                sizeText: "Not tracked", coverUrl: "",
            })
            page.selectedPlayable = true
            wait(30)
            const play = named(page, "playButton")
            compare(play.length, 1)
            verify(play[0].visible)
            verify(play[0].enabled)
            compare(named(page, "detailTitle").length, 1)
        }
    }
}
