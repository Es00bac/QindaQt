// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/apps/file_manager/ui" as Files

TestCase {
    id: testCase
    name: "FileManagerViewport"
    visible: true
    width: 800
    height: 600
    when: windowShown

    QtObject {
        id: navigation
        property string currentPath: "/fixture"
        property var entries: []
        property string statusMessage: ""
        property string sortColumn: "name"
        property string sortDirection: "ascending"
        signal navigationChanged()
        function goUp() {}
        function activate(index) {}
        function setSortColumn(key) {}
    }
    QtObject { id: coordinator; function activateAction(action) {} }
    Files.EntrySelection { id: fixtureSelection; navigationController: navigation }
    Component {
        id: gridComponent
        Files.EntryGrid {
            width: 640; height: 360
            navigationController: navigation
            selection: fixtureSelection
            appCoordinator: coordinator
        }
    }
    Component {
        id: listComponent
        Files.EntryList {
            width: 640; height: 360
            navigationController: navigation
            selection: fixtureSelection
            appCoordinator: coordinator
        }
    }
    QtObject {
        id: places
        property var places: [{id: "home", name: "Home", path: "/fixture"}]
        property var bookmarks: []
        function removeBookmark(index) {}
    }
    Component {
        id: sidebarComponent
        Files.PlacesSidebar {
            width: 196; height: 360
            navigationController: navigation
            placesController: places
            appCoordinator: coordinator
        }
    }
    SignalSpy { id: zoomSpy; signalName: "zoomRequested" }
    property var browser
    property var view
    function init() {
        let entries = []
        for (let i = 0; i < 120; ++i)
            entries.push({name: "File " + String(i).padStart(3, "0"), device: "1",
                inode: String(i), modifiedNanoseconds: "1", isDirectory: false,
                isHidden: false, isSymlink: false, iconName: "text-x-generic",
                sizeText: "1 KB", kindText: "Text", modifiedText: "Today"})
        entries[70].name = "Zebra"
        entries[71].name = "Zinc"
        navigation.entries = entries
        fixtureSelection.selectOnly(0)
    }
    function cleanup() { zoomSpy.target = null; if (browser) browser.destroy(); browser = null }
    function open(mode) {
        browser = (mode === "Grid" ? gridComponent : listComponent).createObject(testCase)
        verify(browser !== null)
        view = findChild(browser, "entry" + mode + "View")
        verify(view !== null)
        browser.focusView()
        zoomSpy.target = browser
        zoomSpy.clear()
        waitForRendering(browser)
    }
    function test_scrollAndZoom_data() { return [{tag:"Grid"}, {tag:"List"}] }
    function test_scrollAndZoom(data) {
        open(data.tag)
        const bar = findChild(browser, "entry" + data.tag + "ScrollBar")
        verify(bar.visible)
        verify(bar.size < 1)
        verify(bar.x >= view.width, "bar=" + bar.x + " view=" + view.width + " parent=" + bar.parent)
        // Ordinary wheel remains scrolling; Ctrl-wheel emits zoom intent only.
        mouseWheel(view, 80, 80, 0, -120, Qt.NoButton, Qt.NoModifier)
        tryVerify(() => view.contentY > 0)
        compare(zoomSpy.count, 0)
        view.cancelFlick()
        const scrollPosition = view.contentY
        mouseWheel(view, 80, 80, 0, 30, Qt.NoButton, Qt.ControlModifier)
        mouseWheel(view, 80, 80, 0, 30, Qt.NoButton, Qt.ControlModifier)
        mouseWheel(view, 80, 80, 0, 30, Qt.NoButton, Qt.ControlModifier)
        compare(zoomSpy.count, 0)
        mouseWheel(view, 80, 80, 0, 30, Qt.NoButton, Qt.ControlModifier)
        compare(zoomSpy.count, 1)
        compare(zoomSpy.signalArguments[0][0], 1)
        compare(view.contentY, scrollPosition)
        mouseWheel(view, 80, 80, 0, -240, Qt.NoButton, Qt.ControlModifier)
        compare(zoomSpy.count, 2)
        compare(zoomSpy.signalArguments[1][0], -2)
        compare(view.contentY, scrollPosition)
        // Dragging the thumb must reach later entries without selecting them.
        view.contentY = 0
        mouseDrag(bar, bar.width / 2, bar.height * bar.visualSize / 2,
                  0, bar.height * 0.6)
        tryVerify(() => view.contentY > 0)
        compare(fixtureSelection.currentIndex, 0)
    }
    function test_keyboardAndResize_data() { return [{tag:"Grid"}, {tag:"List"}] }
    function test_keyboardAndResize(data) {
        open(data.tag)
        keyClick(Qt.Key_PageDown)
        verify(fixtureSelection.currentIndex > 0)
        const pageIndex = fixtureSelection.currentIndex
        keyClick(Qt.Key_PageDown, Qt.ShiftModifier)
        verify(fixtureSelection.count() > 1)
        keyClick(Qt.Key_PageUp)
        compare(fixtureSelection.currentIndex, pageIndex)
        keyClick(Qt.Key_Z)
        compare(fixtureSelection.currentIndex, 70)
        keyClick(Qt.Key_Z)
        compare(fixtureSelection.currentIndex, 71)
        keyClick(Qt.Key_I)
        compare(fixtureSelection.currentIndex, 71)
        const selectedKey = fixtureSelection.currentKey
        browser.iconSize = 128
        waitForRendering(browser)
        compare(fixtureSelection.currentKey, selectedKey)
        verify(view.currentItem.y >= view.contentY - 1, "item=" + view.currentItem.y + " scroll=" + view.contentY)
        verify(view.currentItem.y + view.currentItem.height <= view.contentY + view.height + 1)
        // Manual scroll does not snap back to the focused row on the next frame.
        view.contentY = 0
        wait(50)
        compare(view.contentY, 0)
        browser.width = 500
        waitForRendering(browser)
        verify(view.currentItem.y >= view.contentY - 1, "item=" + view.currentItem.y + " scroll=" + view.contentY)
    }
    function test_bookmarkOverflow() {
        places.bookmarks = Array.from({length: 30}, (_, index) =>
            ({index: index, name: "Bookmark " + index, path: "/fixture/" + index}))
        browser = sidebarComponent.createObject(testCase)
        verify(browser !== null)
        waitForRendering(browser)
        const list = findChild(browser, "bookmarkList")
        const bar = findChild(browser, "bookmarkScrollBar")
        verify(bar.visible)
        verify(bar.x >= list.width)
        mouseDrag(bar, bar.width / 2, bar.height * bar.visualSize / 2,
                  0, bar.height * 0.6)
        tryVerify(() => list.contentY > 0)
        places.bookmarks = []
        tryCompare(bar, "visible", false)
    }
    function test_shortFolderHidesBar_data() { return [{tag:"Grid"}, {tag:"List"}] }
    function test_shortFolderHidesBar(data) {
        navigation.entries = navigation.entries.slice(0, 1)
        open(data.tag)
        const bar = findChild(browser, "entry" + data.tag + "ScrollBar")
        verify(!bar.visible)
    }
}
