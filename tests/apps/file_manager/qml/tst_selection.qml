// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/apps/file_manager/ui" as Files

TestCase {
    name: "FileManagerSelection"
    QtObject {
        id: navigation
        property string currentPath: "/one"
        property var entries: []
        signal navigationChanged()
    }
    Files.EntrySelection { id: selection; navigationController: navigation }
    function entry(name, inode) {
        return {name: name, device: "1", inode: String(inode), modifiedNanoseconds: "100"}
    }
    function names() { return selection.selectedEntries().map(e => e.name).join(",") }
    function init() {
        navigation.currentPath = "/one"
        navigation.entries = [entry("a",1), entry("b",2), entry("c",3), entry("d",4)]
        selection.selectOnly(-1)
    }
    function test_sortAndInsertionPreserveFiles() {
        selection.selectOnly(1); selection.toggle(3)
        navigation.entries = [entry("d",4), entry("a",1), entry("b",2), entry("c",3)]
        compare(names(), "d,b")
        navigation.entries = [entry("new",5)].concat(navigation.entries)
        compare(names(), "d,b")
        compare(selection.currentIndex, 1)
    }
    function test_navigationClearsEvenSameNamesAndIdentities() {
        selection.selectAll()
        navigation.currentPath = "/two"
        // Guards dispatch even before the controller emits its listing signal.
        compare(names(), "")
        navigation.entries = navigation.entries.slice()
        navigation.navigationChanged()
        compare(selection.count(), 0)
    }
    function test_filterDropsHiddenSelectionWithoutResurrection() {
        navigation.entries = [entry(".hidden",5), entry("b",2)]
        selection.selectAll()
        navigation.entries = [entry("b",2)]
        compare(names(), "b")
        navigation.entries = [entry(".hidden",5), entry("b",2)]
        compare(names(), "b")
    }
    function test_replacementDoesNotInheritSelection() {
        selection.selectOnly(1)
        navigation.entries = [entry("a",1), entry("b",99)]
        compare(names(), "")
    }
    function test_mutationReceivesOriginalSnapshot() {
        selection.selectOnly(1)
        let changed = entry("b",2); changed.modifiedNanoseconds = "200"
        navigation.entries = [changed]
        compare(selection.selectedEntries()[0].modifiedNanoseconds, "100")
    }
    function test_keyboardRangeAndFocusAreDistinct() {
        selection.moveTo(0, Qt.NoModifier)
        selection.moveTo(2, Qt.ShiftModifier)
        compare(names(), "a,b,c")
        selection.moveTo(3, Qt.ControlModifier)
        compare(names(), "a,b,c"); compare(selection.currentIndex, 3)
        selection.moveTo(1, Qt.NoModifier)
        compare(names(), "b")
    }
    function test_anchorFollowsSort() {
        selection.selectOnly(1)
        navigation.entries = [entry("c",3),entry("b",2),entry("a",1),entry("d",4)]
        selection.rangeTo(3)
        compare(names(), "b,a,d")
    }
    function test_deselectLastDoesNotFallbackToCurrent() {
        selection.selectOnly(1); selection.toggle(1)
        compare(names(), ""); compare(selection.currentIndex, 1)
    }
}
