// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest
import "../../../../src/apps/file_manager/ui" as Files

TestCase {
    id: testCase
    name: "FileManagerMutationOutput"
    width: 900
    height: 650
    visible: true
    when: windowShown

    QtObject {
        id: mutation
        property bool busy: false
        property int progressValue: 0
        property string progressText: ""
        property string failureCode: "cancelled"
        property string failureMessage: "Copy cancelled"
        property string outputNotice: "Partial copy retained at last observation: /tmp/<b>literal & output</b>"
        property string resultText: ""
        property bool canRestore: false
        property bool canUndo: false
        function clearFailure() { failureCode = "none"; outputNotice = "" }
    }
    Files.StatusBanners {
        id: banners
        width: testCase.width
        navigationController: ({ applicationsPlace: false, launchError: "" })
        mutationController: mutation
        placesController: ({ storeError: "" })
        networkLocationsController: ({ storeError: "" })
        transferQueueController: ({
            busy: false, failedCount: 0, refusal: "", activeDescription: "",
            activePercent: 0, queuedCount: 0, activePaused: false
        })
    }

    function allVisualItems(item) {
        let items = [item]
        for (const child of item.children)
            items = items.concat(allVisualItems(child))
        return items
    }
    function test_failureRetainsLiteralPathAndNoRecoveryAction() {
        const card = findChild(banners, "mutationFailureCard")
        verify(card !== null)
        verify(card.visible)
        compare(card.message, mutation.failureMessage + "\n" + mutation.outputNotice)
        verify(card.Accessible.name.indexOf("<b>literal & output</b>") >= 0)
        let titleSeen = false
        let messageSeen = false
        for (const child of allVisualItems(card.contentItem)) {
            if (child.text === card.title) {
                compare(child.textFormat, Text.PlainText)
                titleSeen = true
            }
            if (child.text === card.message) {
                compare(child.textFormat, Text.PlainText)
                messageSeen = true
            }
        }
        verify(titleSeen)
        verify(messageSeen)
        compare(card.actionText, "Dismiss")
        verify(!findChild(banners, "mutationResultCard").visible)
        card.actionTriggered()
        compare(mutation.failureCode, "none")
        verify(!card.visible)
    }
}
