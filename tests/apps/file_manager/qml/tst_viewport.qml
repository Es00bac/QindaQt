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
        property string statusKey: "ready"
        property string nameFilter: ""
        property int maximumNameFilterLength: 256
        property bool guestListingActive: false
        property bool showHidden: false
        signal navigationChanged()
        function goUp() {}
        function activate(index) {}
        function setSortColumn(key) {}
        function setNameFilter(text) { nameFilter = text }
        function clearGuestListing() { guestListingActive = false }
        function navigateTo(path) { currentPath = path; statusKey = "loading"; navigationChanged() }
    }
    QtObject {
        id: search
        property int requests: 0
        function cancel() {}
        function startSearch(path, query, hidden) { requests++ }
    }
    Component {
        id: filterComponent
        Files.FilterBar { width: 640; navigationController: navigation; searchController: search }
    }
    Component {
        id: locationComponent
        Files.LocationBar { width: 640; navigationController: navigation }
    }
    SignalSpy { id: locationClosed; signalName: "closed" }
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
    QtObject {
        id: networkLocations
        property var placesLocations: []
        property var locations: []
        property string storeError: ""
        function clearStoreError() {}
    }
    Component {
        id: sidebarComponent
        Files.PlacesSidebar {
            width: 196; height: 360
            navigationController: navigation
            placesController: places
            appCoordinator: coordinator
            networkLocationsController: networkLocations
        }
    }
    SignalSpy { id: zoomSpy; signalName: "zoomRequested" }
    property var browser
    property var view
    function init() {
        navigation.currentPath = "/fixture"
        navigation.statusKey = "ready"
        navigation.nameFilter = ""
        navigation.guestListingActive = false
        search.requests = 0
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
    function test_locationWaitsForListing() {
        const bar = createTemporaryObject(locationComponent, testCase)
        verify(bar)
        locationClosed.target = bar
        locationClosed.clear()
        bar.activate()
        const field = findChild(bar, "locationField")
        field.text = "sftp://fixture/folder"
        keyClick(Qt.Key_Return)
        compare(locationClosed.count, 0)
        navigation.statusKey = "ready"
        navigation.entriesChanged()
        compare(locationClosed.count, 1)
        locationClosed.target = null
    }
    function test_recursiveFilterClearsAndCancelsDebounce() {
        const bar = createTemporaryObject(filterComponent, testCase)
        verify(bar)
        const toggle = findChild(bar, "filterSubfoldersToggle")
        toggle.checked = true
        const field = findChild(bar, "folderFilterField")
        bar.activate()
        for (const key of [Qt.Key_M, Qt.Key_A, Qt.Key_T, Qt.Key_C, Qt.Key_H]) keyClick(key)
        bar.visible = false
        wait(350)
        compare(search.requests, 0)
        bar.visible = true
        bar.activate()
        navigation.guestListingActive = true
        keyClick(Qt.Key_Backspace)
        verify(!navigation.guestListingActive)
        for (const key of [Qt.Key_N, Qt.Key_E, Qt.Key_W]) keyClick(key)
        navigation.currentPath = "/another-folder"
        wait(350)
        compare(search.requests, 0)
    }
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
    function delegateByIndex(index) {
        for (let i = 0; i < view.contentItem.children.length; ++i) {
            const child = view.contentItem.children[i]
            if (child.index === index)
                return child
        }
        return null
    }
    function test_marqueeSelection_data() { return [{tag:"Grid"}, {tag:"List"}] }
    function test_marqueeSelection(data) {
        navigation.entries = navigation.entries.slice(0, 2)
        fixtureSelection.selectOnly(0)
        open(data.tag)
        const band = findChild(browser, "selectionBand")
        verify(band !== null)
        // A band dragged across both entries selects exactly them.
        mouseDrag(band, 20, 300, 560, -260)
        compare(fixtureSelection.count(), 2)
        compare(fixtureSelection.currentIndex, 1)
        // A plain click on empty space clears the selection.
        mouseClick(band, 20, 300)
        compare(fixtureSelection.count(), 0)
        // Shift-drag unions with the current selection.
        fixtureSelection.selectOnly(1)
        mouseDrag(band, 20, 300, 560, -260, Qt.LeftButton, Qt.ShiftModifier)
        compare(fixtureSelection.count(), 2)
        // Ctrl-drag toggles every crossed entry: starting from only entry 0,
        // entry 0 drops out and entry 1 joins.
        fixtureSelection.selectOnly(0)
        mouseDrag(band, 20, 300, 560, -260, Qt.LeftButton, Qt.ControlModifier)
        compare(fixtureSelection.count(), 1)
        verify(!fixtureSelection.isSelected(0))
        verify(fixtureSelection.isSelected(1))
        // Presses on a delegate never start a band: ordinary clicking below
        // keeps working and the band reports nothing.
        fixtureSelection.selectOnly(0)
        const first = delegateByIndex(0)
        mouseDrag(first, 5, 5, 30, 30)
        compare(fixtureSelection.count(), 1)
        verify(fixtureSelection.isSelected(0))
    }
    function test_detailsRowsAlternateBackground() {
        open("List")
        fixtureSelection.selected = ({})
        const even = delegateByIndex(0)
        const odd = delegateByIndex(1)
        verify(even !== null && odd !== null)
        // Even rows rest transparent; odd rows carry the palette's
        // alternateBase so adjacent rows are easy to tell apart.
        compare(String(even.color), "#00000000")
        compare(String(odd.color), String(browser.palette.alternateBase))
        // Hover is a translucent highlight tint on either parity.
        const highlight = browser.palette.highlight
        const hoverTint = Qt.rgba(highlight.r, highlight.g, highlight.b, 0.20)
        mouseMove(odd, 4, 4)
        tryCompare(odd, "color", hoverTint)
        mouseMove(even, 4, 4)
        tryCompare(even, "color", hoverTint)
        // Move the pointer into empty trailing viewport space (only two
        // fixture entries) so the hover highlight clears entirely.
        const band = findChild(browser, "selectionBand")
        mouseMove(band, band.width - 4, band.height - 4)
        tryCompare(even, "color", "#00000000")
        // Selection still wins over both stripes and hover.
        fixtureSelection.selectOnly(1)
        tryCompare(odd, "color", browser.palette.highlight)
    }
}
