// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest
import "../../../../src/apps/file_manager/ui" as Files

TestCase {
    id: testCase
    name: "FileManagerRemoteContext"
    width: 800
    height: 600
    visible: true
    when: windowShown

    QtObject {
        id: navigation
        property bool remoteActive: true
        property bool applicationsPlace: false
        property bool showHidden: false
        property string currentPath: "sftp://fixture.invalid/folder"
    }
    QtObject {
        id: coordinator
        property var menus: []
        property var recorded: []
        // The real coordinator also rejects unknown or currently disabled IDs.
        function activateAction(id) {
            const actions = menus[0].actions
            for (let i = 0; i < actions.length; ++i) {
                if (actions[i].id === id && actions[i].enabled) {
                    recorded = recorded.concat([id])
                    return
                }
            }
        }
    }
    QtObject {
        id: localActions
        property bool inTrash: false
        property var templates: []
        function describeSelection() { return {files: 1, folders: 0, archives: 0} }
        function openWithCandidates() { return [] }
        function refreshTemplates() {}
    }
    Files.FileContextMenu {
        id: contextMenu
        parent: testCase
        appCoordinator: coordinator
        navigationController: navigation
        fileActions: localActions
        selectionCount: 1
    }

    function menuItem(name) {
        for (let i = 0; i < contextMenu.count; ++i) {
            const candidate = contextMenu.itemAt(i)
            if (candidate && candidate.objectName === name)
                return candidate
        }
        fail("Missing context item: " + name)
    }
    function setActions(admitted) {
        const ids = ["file.open", "file.copy", "file.move", "file.rename", "file.new-folder"]
        const actions = ids.map(id => ({id: id, enabled: admitted}))
        // Deliberately permissive Trash catalog: the unsupported remote
        // presentation must still refuse to dispatch its local operation.
        actions.push({id: "file.trash", enabled: true})
        coordinator.menus = [{actions: actions}]
    }
    function init() {
        navigation.remoteActive = true
        navigation.applicationsPlace = false
        contextMenu.selectionCount = 1
        coordinator.recorded = []
        contextMenu.currentIndex = -1
        contextMenu.contentItem.positionViewAtBeginning()
        setActions(true)
    }
    function closeMenu() {
        contextMenu.close()
        tryCompare(contextMenu, "opened", false)
    }
    function cleanup() { closeMenu() }
    function openMenu() {
        contextMenu.popup(10, 10)
        tryCompare(contextMenu, "opened", true)
        verify(waitForRendering(contextMenu.contentItem))
    }
    function test_remoteCommandsUseExistingCatalog() {
        const names = ["contextOpenAction", "contextCopyAction", "contextMoveAction", "contextRenameAction"]
        for (let i = 0; i < names.length; ++i) {
            openMenu()
            const action = menuItem(names[i])
            verify(action.visible)
            verify(action.enabled)
            mouseClick(action, action.width / 2, action.height / 2)
            tryCompare(coordinator, "recorded", ["file.open", "file.copy", "file.move", "file.rename"].slice(0, i + 1))
            closeMenu()
        }
    }
    function test_busyCatalogDisablesRemoteCommands() {
        setActions(false)
        openMenu()
        for (const name of ["contextOpenAction", "contextCopyAction", "contextMoveAction", "contextRenameAction"]) {
            const action = menuItem(name)
            verify(!action.enabled)
            mouseClick(action, action.width / 2, action.height / 2)
        }
        compare(coordinator.recorded.length, 0)
    }
    function test_remoteTrashExplainsAndRefusesDispatch() {
        openMenu()
        const action = menuItem("contextTrashAction")
        verify(action.visible)
        verify(!action.enabled)
        verify(action.text.indexOf("unavailable for remote") >= 0)
        mouseClick(action, action.width / 2, action.height / 2)
        compare(coordinator.recorded.length, 0)
        // A stale/programmatic activation cannot bypass the explicit guard.
        action.triggered()
        compare(coordinator.recorded.length, 0)
    }
    function test_localTrashKeepsAdmittedAction() {
        navigation.remoteActive = false
        openMenu()
        const action = menuItem("contextTrashAction")
        verify(action.visible)
        verify(action.enabled)
        compare(action.text, "Move to Trash")
        // The local menu is taller than this compact viewport. Scroll its
        // actual ListView before dispatching a pointer to the admitted row.
        for (let i = 0; i < contextMenu.count; ++i) {
            if (contextMenu.itemAt(i) === action) {
                contextMenu.contentItem.positionViewAtIndex(i, ListView.Contain)
                break
            }
        }
        wait(0)
        verify(action.y - contextMenu.contentItem.contentY >= 0)
        verify(action.y - contextMenu.contentItem.contentY + action.height <= contextMenu.contentItem.height)
        mouseClick(action, action.width / 2, action.height / 2)
        tryCompare(coordinator, "recorded", ["file.trash"])
    }
    function test_remoteBackgroundAndMultipleSelection() {
        contextMenu.selectionCount = 0
        openMenu()
        const folder = menuItem("contextNewFolderAction")
        verify(folder.visible)
        verify(folder.enabled)
        verify(!menuItem("contextRenameAction").visible)
        mouseClick(folder, folder.width / 2, folder.height / 2)
        tryCompare(coordinator, "recorded", ["file.new-folder"])
        closeMenu()
        contextMenu.selectionCount = 2
        openMenu()
        verify(!menuItem("contextRenameAction").visible)
    }
}
