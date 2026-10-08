// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest
import "../../../../src/apps/file_manager/ui" as Files
TestCase {
    id: testCase
    name: "FileManagerTrashRestore"
    width: 900
    height: 650
    visible: true
    when: windowShown
    QtObject {
        id: controller
        property bool busy: false
        property string failureMessage: "Original <b>& folder is unavailable"
        property bool admitted: false
        property string action: ""
        property string chosen: ""
        function putBackItems(items) { action = "original-items"; return admitted }
        function putBackItemsTo(items, folder) { action = "chosen-items"; chosen = folder; return admitted }
        function restoreLast() { action = "original-last"; return admitted }
        function restoreLastTo(folder) { action = "chosen-last"; chosen = folder; return admitted }
    }
    Files.TrashRestoreDialog {
        id: dialog
        parent: testCase
        controller: controller
        originalPath: "/volume/<b>literal & original</b>"
    }
    function init() {
        dialog.close()
        controller.busy = false; controller.admitted = false
        controller.action = ""; controller.chosen = ""
        dialog.items = []; dialog.initialFolder = "/volume/<b>chosen & folder</b>"
    }
    function test_literalOriginalAndRefusal() {
        dialog.open(); tryCompare(dialog, "opened", true)
        dialog.restore(false)
        compare(controller.action, "original-last")
        verify(dialog.visible)
        compare(dialog.refusal, controller.failureMessage)
        let originals = 0, refusals = 0
        for (const item of dialog.contentItem.children) {
            if (item.text === dialog.originalPath) { compare(item.textFormat, Text.PlainText); originals++ }
            if (item.text === dialog.refusal) { compare(item.textFormat, Text.PlainText); refusals++ }
        }
        compare(originals, 1); compare(refusals, 1)
    }
    function test_deliberateChosenFolderForItems() {
        dialog.items = [{path: "/volume/.Trash/files/item"}]
        controller.admitted = true
        dialog.open(); tryCompare(dialog, "opened", true)
        const button = findChild(dialog, "trashRestoreChosen")
        verify(button.enabled); mouseClick(button)
        compare(controller.action, "chosen-items")
        compare(controller.chosen, "/volume/<b>chosen & folder</b>")
        tryCompare(dialog, "visible", false)
    }
    function test_busyRefusesButtons() {
        controller.busy = true; dialog.open(); tryCompare(dialog, "opened", true)
        verify(!findChild(dialog, "trashRestoreChosen").enabled)
        verify(!findChild(dialog, "trashRestoreOriginal").enabled)
        compare(controller.action, "")
    }
    function test_keyboardChosenFolderLast() {
        controller.admitted = true; dialog.open(); tryCompare(dialog, "opened", true)
        const field = findChild(dialog, "trashRestoreFolder")
        compare(field.Accessible.name, "Existing local restore folder")
        field.forceActiveFocus(); keyClick(Qt.Key_Return)
        compare(controller.action, "chosen-last")
        compare(controller.chosen, "/volume/<b>chosen & folder</b>")
    }
}
