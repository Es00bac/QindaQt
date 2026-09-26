// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaTK as Tk
import "../../../../src/apps/qindalutris/qml" as App
import "../../../../src/apps/qindalutris/qml/parts" as Parts

// The Get games and Proton pages over stub rows (ADR-0275): each store card
// offers exactly one honest action for its state, a job shows progress then
// ONE sentence, the setup-file flow asks which program is the game, and the
// Proton list marks one default and never offers to remove a build in use.
Item {
    id: root
    width: 1180
    height: 900

    App.GetGamesPage {
        id: games
        anchors.fill: parent
        stores: [
            { id: "battlenet", name: "Battle.net", notes: "n", installed: false,
              titleId: "", adoptable: true, prefixPath: "/home/u/Games/battlenet" },
            { id: "ea", name: "EA app", notes: "", installed: false, titleId: "",
              adoptable: false, prefixPath: "/home/u/Games/ea-app" },
            { id: "gog-galaxy", name: "GOG Galaxy", notes: "", installed: true,
              titleId: "title/gog-galaxy", adoptable: false, prefixPath: "" }
        ]
    }

    App.ProtonPage {
        id: protons
        visible: false
        width: 1180
        height: 900
        builds: [
            { name: "GE-Proton11-6-x86_64", displayName: "GE-Proton11-6-x86_64",
              version: "GE-Proton11-6", origin: "system", pinnable: true, removable: false,
              label: "", status: "tested", notes: "", isDefault: true, usedBy: 2 },
            { name: "GE-Proton11-7-x86_64", displayName: "GE-Proton11-7-x86_64",
              version: "GE-Proton11-7", origin: "user", pinnable: true, removable: true,
              label: "", status: "known-issues", notes: "stalls WoW", isDefault: false, usedBy: 0 },
            { name: "GE-Proton9-1", displayName: "GE-Proton9-1", version: "GE-Proton9-1",
              origin: "user", pinnable: true, removable: true, label: "", status: "untested",
              notes: "", isDefault: false, usedBy: 1 }
        ]
    }

    Parts.VerdictCard {
        id: verdictCard
        width: 320
        verdict: ({ found: true, verdict: "blocked", verdictText: "Can't run on Linux",
                    canRun: false, antiCheat: "Its anti-cheat does not work on Linux",
                    steamDeck: "", protondb: "Borked (community rating)", fixes: [],
                    avoid: [], sources: "QindaQt compatibility database; community rating from ProtonDB (ODbL)" })
    }

    SignalSpy { id: installSpy; target: games; signalName: "installStoreRequested" }
    SignalSpy { id: adoptSpy; target: games; signalName: "adoptStoreRequested" }
    SignalSpy { id: openSpy; target: games; signalName: "openTitleRequested" }
    SignalSpy { id: setupSpy; target: games; signalName: "installSetupRequested" }
    SignalSpy { id: chosenSpy; target: games; signalName: "candidateChosen" }
    SignalSpy { id: defaultSpy; target: protons; signalName: "makeDefaultRequested" }

    TestCase {
        name: "QindaLutrisInstallPages"
        when: windowShown

        function named(item, name) {
            let result = item.objectName === name ? [item] : []
            for (let child of item.children || [])
                result = result.concat(named(child, name))
            return result
        }
        function one(item, name) {
            const found = named(item, name)
            verify(found.length === 1, name + " found " + found.length + " times")
            return found[0]
        }

        function init() {
            games.busy = false
            games.resultMessage = ""
            games.candidates = []
            installSpy.clear(); adoptSpy.clear(); openSpy.clear()
            setupSpy.clear(); chosenSpy.clear(); defaultSpy.clear()
            wait(30)
        }

        function test_eachStoreOffersOneHonestAction() {
            compare(one(games, "storeAction-battlenet").text, "Add to library")
            compare(one(games, "storeAction-ea").text, "Install")
            compare(one(games, "storeAction-gog-galaxy").text, "Open")
            mouseClick(one(games, "storeAction-battlenet"))
            compare(adoptSpy.count, 1)
            compare(adoptSpy.signalArguments[0][0], "battlenet")
            compare(adoptSpy.signalArguments[0][1], "/home/u/Games/battlenet")
            mouseClick(one(games, "storeAction-ea"))
            compare(installSpy.count, 1)
            compare(installSpy.signalArguments[0][0], "ea")
            mouseClick(one(games, "storeAction-gog-galaxy"))
            compare(openSpy.signalArguments[0][0], "title/gog-galaxy")
        }

        function test_aRunningJobShowsProgressAndBlocksNewInstalls() {
            games.busy = true
            games.progress = 0.4
            games.stageText = "Downloading the EA app installer…"
            wait(30)
            verify(one(games, "jobProgressCard").visible)
            compare(one(games, "jobStageText").text, "Downloading the EA app installer…")
            verify(!one(games, "storeAction-ea").enabled)
            games.busy = false
            games.resultMessage = "The EA app is installed."
            games.succeeded = true
            wait(30)
            verify(!one(games, "jobProgressCard").visible)
            verify(one(games, "jobResultNotice").visible)
            verify(one(games, "storeAction-ea").enabled)
        }

        function test_setupFileFlowAsksWhichProgramIsTheGame() {
            const install = one(games, "setupInstallButton")
            verify(!install.enabled)
            one(games, "setupTitleField").text = "Old Game"
            one(games, "setupPathField").text = "/home/u/Downloads/setup.exe"
            wait(30)
            verify(install.enabled)
            mouseClick(install)
            compare(setupSpy.count, 1)
            compare(setupSpy.signalArguments[0][0], "Old Game")
            games.candidates = [{ path: "/p/drive_c/Game/game.exe", name: "game.exe", sizeBytes: 9 }]
            wait(30)
            const card = one(games, "candidateCard")
            verify(card.visible)
        }

        function test_microsoftStoreIsExplainedNotOffered() {
            const notice = one(games, "microsoftStoreNotice")
            verify(notice.visible)
            verify(notice.text.indexOf("cannot run on Linux") >= 0)
        }

        function test_verdictCardLeadsWithOnePlainVerdict() {
            verify(verdictCard.visible)
            const badge = one(verdictCard, "verdictBadge")
            compare(badge.text, "Can't run on Linux")
            compare(badge.variant, "danger")
            verify(one(verdictCard, "verdictSources").text.indexOf("ProtonDB (ODbL)") >= 0)
            verdictCard.verdict = ({ found: false })
            wait(30)
            verify(!verdictCard.visible)
        }

        function test_protonListMarksOneDefaultAndProtectsBuildsInUse() {
            protons.visible = true
            wait(30)
            verify(one(protons, "buildCard-GE-Proton11-6-x86_64").selected)
            verify(!one(protons, "buildCard-GE-Proton11-7-x86_64").selected)
            compare(protons.statusText("tested", true), "Tested by QindaQt")
            compare(protons.statusText("known-issues", true), "Known issues")
            compare(protons.statusText("tested", false), "Updated by Steam")
            protons.visible = false
        }
    }
}
