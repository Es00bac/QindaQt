// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest
import "../../../../src/apps/file_manager/ui" as Files
TestCase {
    id: root
    name: "FileManagerMoveRecovery"
    width: 900
    height: 800
    visible: true
    when: windowShown
    QtObject {
        id: mutation
        property bool busy: false
        property int inspections: 0
        property string restoredId: ""
        property var recoveryRecords: [{
            operationId: "operation-uuid", phase: "retained",
            retainedBytes: "18446744073709551615",
            sourcePath: "/source/<b>literal & path</b>",
            destinationPath: "/destination/<i>literal</i>",
            recoveryDirectory: "/source/.recovery", stageDirectory: "/dest/.stage",
            canRestore: true, uncertain: false
        }]
        function inspectRecovery() { inspections++ }
        function restoreRecovery(id) { restoredId = id }
    }
    Files.MoveRecoveryPanel {
        id: panel
        width: parent.width
        mutationController: mutation
    }
    function descendants(item) {
        let result = [item]
        for (const child of item.children)
            result = result.concat(descendants(child))
        return result
    }
    function test_discoveryIsInspectionOnlyAndPathsStayLiteral() {
        verify(mutation.inspections >= 1)
        compare(mutation.restoredId, "")
        const card = findChild(panel, "moveRecoveryReceipt")
        verify(card !== null)
        verify(card.message.indexOf("<b>literal & path</b>") >= 0)
        verify(card.message.indexOf("18446744073709551615") >= 0)
        verify(card.Accessible.name.indexOf("<b>literal & path</b>") >= 0)
        let literalLabel = false
        for (const child of descendants(card)) {
            if (child.text === card.message) {
                compare(child.textFormat, Text.PlainText)
                literalLabel = true
            }
        }
        verify(literalLabel)
        const inspect = findChild(panel, "inspectMoveRecoveryButton")
        verify(inspect.enabled)
        mouseClick(inspect)
        verify(mutation.inspections >= 2)
        compare(mutation.restoredId, "")
    }
    function test_restoreRequiresConfirmationAndOnlyPassesCatalogId() {
        const card = findChild(panel, "moveRecoveryReceipt")
        card.actionTriggered()
        const dialog = findChild(panel, "moveRecoveryRestoreConfirmation")
        verify(dialog.opened)
        compare(mutation.restoredId, "")
        dialog.reject()
        compare(mutation.restoredId, "")
        card.actionTriggered()
        dialog.accept()
        compare(mutation.restoredId, "operation-uuid")
        mutation.busy = true
        compare(card.actionText, "")
        verify(!findChild(panel, "inspectMoveRecoveryButton").enabled)
        mutation.busy = false
    }
}
