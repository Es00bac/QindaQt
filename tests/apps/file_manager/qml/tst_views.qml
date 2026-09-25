// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/apps/file_manager/ui" as Files

// ADR-0270: the Details view's headings and column edits, the Columns view's
// folder levels and the Gallery view's strip, over a fixture navigation.
TestCase {
    id: testCase
    name: "FileManagerViews"
    visible: true
    width: 900
    height: 600
    when: windowShown

    QtObject {
        id: navigation
        property string currentPath: "/fixture/inner"
        property var entries: []
        property string statusMessage: ""
        property string sortColumn: "name"
        property string sortDirection: "ascending"
        property string statusKey: "ready"
        property string groupBy: "none"
        property string viewMode: "list"
        property bool showHidden: false
        property bool directoriesFirst: true
        property bool canGoUp: true
        property bool guestListingActive: false
        property var visited: []
        property int upRequests: 0
        property var activated: []
        signal navigationChanged()
        function goUp() { upRequests++ }
        function activate(index) { activated = activated.concat([index]) }
        function setSortColumn(key) { sortColumn = key }
        function setGroupBy(key) { groupBy = key }
        function navigateTo(path) { visited = visited.concat([path]) }
        function indexOfName(name) {
            for (let i = 0; i < entries.length; ++i)
                if (entries[i].name === name) return i
            return -1
        }
    }
    QtObject { id: coordinator; function activateAction(action) {} }
    QtObject {
        id: listing
        property var requests: []
        function children(path, showHidden, column, direction, foldersFirst) {
            requests = requests.concat([path])
            return [{ name: "inner", path: path === "/" ? "/fixture" : path + "/inner",
                      isDirectory: true, isHidden: false, iconName: "folder" },
                    { name: "other.txt", path: path + "/other.txt", isDirectory: false,
                      isHidden: false, iconName: "text-x-generic" }]
        }
    }
    Files.EntrySelection { id: selection; navigationController: navigation }

    Component {
        id: detailsComponent
        Files.DetailsView {
            width: 800; height: 500
            navigationController: navigation; selection: selection; appCoordinator: coordinator
        }
    }
    Component {
        id: columnsComponent
        Files.ColumnsView {
            width: 900; height: 500; active: true; columnListing: listing
            navigationController: navigation; selection: selection; appCoordinator: coordinator
        }
    }
    Component {
        id: galleryComponent
        Files.GalleryView {
            width: 900; height: 500; active: true
            navigationController: navigation; selection: selection; appCoordinator: coordinator
        }
    }

    function entry(name, directory, group) {
        return { name: name, path: "/fixture/inner/" + name, device: "1", inode: name,
                 modifiedNanoseconds: "1", isDirectory: directory, isHidden: false,
                 isSymlink: false, iconName: directory ? "folder" : "text-x-generic",
                 sizeText: directory ? "—" : "1 KB", kindText: directory ? "Folder" : "Text",
                 group: group }
    }
    function init() {
        navigation.groupBy = "none"
        navigation.visited = []
        navigation.upRequests = 0
        navigation.activated = []
        navigation.entries = [entry("docs", true, "Folder"), entry("a.txt", false, "TXT File"),
                              entry("b.txt", false, "TXT File"), entry("c.log", false, "LOG File")]
        selection.selectOnly(0)
    }

    function test_detailsHeadingsFollowGroupBy() {
        const view = createTemporaryObject(detailsComponent, testCase)
        verify(view !== null)
        compare(view.headingCount, 0)
        compare(view.rowModel.rows.length, 4)
        // Group By heads each group once, and entries keep their indexes.
        navigation.groupBy = "kind"
        navigation.entries = [entry("docs", true, "Folder"), entry("c.log", false, "LOG File"),
                              entry("a.txt", false, "TXT File"), entry("b.txt", false, "TXT File")]
        compare(view.headingCount, 3)
        compare(view.rowModel.rows.length, 7)
        compare(view.rowModel.rows[0].heading, "Folder")
        compare(view.rowModel.rowOf[2], 5)
        compare(view.rowModel.rows[view.rowModel.rowOf[2]], 2)
    }

    function test_detailsColumnEditsKeepNameFirst() {
        const view = createTemporaryObject(detailsComponent, testCase)
        const columns = findChild(view, "detailsColumnSet")
        verify(columns !== null)
        compare(columns.shownKeys, "name,size,kind,modified")
        columns.showColumn("owner", true)
        compare(columns.shownKeys, "name,size,kind,modified,owner")
        columns.moveColumn("owner", -1)
        compare(columns.shownKeys, "name,size,kind,owner,modified")
        // Name never moves and never hides; nothing moves before it.
        columns.moveColumn("size", -1)
        columns.showColumn("name", false)
        compare(columns.shownKeys, "name,size,kind,owner,modified")
        columns.showColumn("kind", false)
        compare(columns.shownKeys, "name,size,owner,modified")
        columns.resizeColumn("size", 20)
        compare(columns.widthMap["size"], 40)
        // The header's right-click opens the chooser.
        view.openColumnChooser()
        const chooser = findChild(view, "columnChooser")
        tryVerify(() => chooser.visible)
        chooser.close()
    }

    function test_columnsWalkFolderLevels() {
        const view = createTemporaryObject(columnsComponent, testCase)
        verify(view !== null)
        // "/" and "/fixture" are the levels above "/fixture/inner".
        compare(view.ancestors.length, 2)
        compare(view.ancestors[1].path, "/fixture")
        compare(view.ancestors[1].childPath, "/fixture/inner")
        verify(listing.requests.indexOf("/fixture") >= 0)
        view.focusView()
        const list = findChild(view, "entryColumnsView")
        tryVerify(() => list.activeFocus)
        // Right opens the current folder; Left goes up a level.
        keyClick(Qt.Key_Right)
        compare(navigation.visited, ["/fixture/inner/docs"])
        keyClick(Qt.Key_Left)
        compare(navigation.upRequests, 1)
        // Down moves through the browsed column as in every view.
        keyClick(Qt.Key_Down)
        compare(selection.currentIndex, 1)
        // A file in a column above opens its folder with the file selected.
        view.openFrom("/fixture", { name: "other.txt", isDirectory: false })
        compare(navigation.visited[navigation.visited.length - 1], "/fixture")
    }

    function test_galleryStripSelectsAndOpens() {
        const view = createTemporaryObject(galleryComponent, testCase)
        verify(view !== null)
        const strip = findChild(view, "galleryStrip")
        verify(strip !== null)
        tryVerify(() => strip.windowEntries.length === 4)
        // A click on the third frame makes it current and selected.
        const slot = strip.frameWidth + strip.gap
        mouseClick(strip, slot * 2 + slot / 2, strip.height / 2)
        compare(selection.currentIndex, 2)
        verify(selection.isSelected(2))
        // Ctrl-click adds, as in every view.
        mouseClick(strip, slot / 2, strip.height / 2, Qt.LeftButton, Qt.ControlModifier)
        compare(selection.count(), 2)
        // The facts beside the stage name the current entry.
        const name = findChild(view, "entryFactsName")
        compare(name.text, "docs")
        // Arrows walk the folder; Return opens the current entry.
        view.focusView()
        keyClick(Qt.Key_Right)
        compare(selection.currentIndex, 1)
        keyClick(Qt.Key_Return)
        compare(navigation.activated, [1])
    }
}
