// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Explorer style's folder tree (ADR-0271), under the sidebar's places:
// the home folder and the file system as two roots, opened down to the
// folder being browsed. A row opens its folder in the active tab; its arrow
// (or Right and Left) shows or hides its subfolders. Rows scroll with the
// rest of the sidebar.
//
// AGENT-NOTE: the rows come from ColumnListing (ADR-0270): one bounded
// synchronous read per open folder, re-read whenever the tree is rebuilt
// (a navigation, or a folder opened or closed), so the tree never keeps a
// stale copy of a folder. Local folders only; a network location shows no
// branch of its own.
FocusScope {
    id: root
    objectName: "folderTree"

    required property var navigationController
    // ColumnListing; without one the tree shows its roots only.
    property var columnListing: null
    property string homePath: ""

    // Open folders by path; the rows are the open tree, flattened.
    property var expanded: ({})
    property var rows: []
    property int currentRow: -1

    implicitHeight: column.implicitHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.Tree
    Accessible.name: qsTr("Folders")

    function subfolders(path) {
        if (!root.columnListing)
            return []
        return root.columnListing.children(path, root.navigationController.showHidden,
                                           "name", "ascending", true)
            .filter(entry => entry.isDirectory)
    }

    function rebuild() {
        const next = []
        const visit = (path, name, depth) => {
            const open = root.expanded[path] === true
            next.push({ "path": path, "name": name, "depth": depth, "open": open })
            if (!open)
                return
            for (const child of root.subfolders(path))
                visit(child.path, child.name, depth + 1)
        }
        if (root.homePath.length > 0)
            visit(root.homePath, qsTr("Home"), 0)
        visit("/", qsTr("File System"), 0)
        root.rows = next
        const current = next.findIndex(row => row.path === root.navigationController.currentPath)
        if (current >= 0)
            root.currentRow = current
        else if (root.currentRow >= next.length)
            root.currentRow = next.length - 1
    }

    // Opens every folder above the browsed one, under Home when it is there.
    function reveal() {
        const path = root.navigationController.currentPath
        if (!path.startsWith("/"))
            return root.rebuild()
        const inHome = root.homePath.length > 0
            && (path === root.homePath || path.startsWith(root.homePath + "/"))
        const top = inHome ? root.homePath : "/"
        const next = Object.assign({}, root.expanded)
        let ancestor = path
        while (ancestor !== top && ancestor.length > top.length) {
            ancestor = ancestor.substring(0, ancestor.lastIndexOf("/")) || "/"
            next[ancestor] = true
        }
        root.expanded = next
        root.rebuild()
    }

    function setOpen(index, open) {
        const row = root.rows[index]
        if (!row || row.open === open)
            return
        const next = Object.assign({}, root.expanded)
        if (open)
            next[row.path] = true
        else
            delete next[row.path]
        root.expanded = next
        root.rebuild()
    }

    function openRow(index) {
        const row = root.rows[index]
        if (!row)
            return
        root.currentRow = index
        root.navigationController.navigateTo(row.path)
    }

    // The nearest row above `index` one level up, or -1.
    function parentRow(index) {
        const depth = root.rows[index].depth
        for (let i = index - 1; i >= 0; --i) {
            if (root.rows[i].depth < depth)
                return i
        }
        return -1
    }

    Connections {
        target: root.navigationController
        function onNavigationChanged() { root.reveal() }
    }
    onNavigationControllerChanged: root.reveal()
    Component.onCompleted: root.reveal()

    Keys.onPressed: (event) => {
        const row = root.rows[root.currentRow]
        if (event.key === Qt.Key_Down) {
            root.currentRow = Math.min(root.rows.length - 1, root.currentRow + 1)
        } else if (event.key === Qt.Key_Up) {
            root.currentRow = Math.max(0, root.currentRow - 1)
        } else if (event.key === Qt.Key_Right && row) {
            if (!row.open)
                root.setOpen(root.currentRow, true)
            else if (root.currentRow + 1 < root.rows.length
                     && root.rows[root.currentRow + 1].depth > row.depth)
                root.currentRow += 1
        } else if (event.key === Qt.Key_Left && row) {
            if (row.open)
                root.setOpen(root.currentRow, false)
            else if (root.parentRow(root.currentRow) >= 0)
                root.currentRow = root.parentRow(root.currentRow)
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                   || event.key === Qt.Key_Space) {
            root.openRow(root.currentRow)
        } else {
            return
        }
        event.accepted = true
    }

    ColumnLayout {
        id: column
        width: root.width
        spacing: 0

        Repeater {
            model: root.rows

            ItemDelegate {
                id: rowItem
                required property var modelData
                required property int index
                objectName: "folderTreeRow_" + modelData.path
                Layout.fillWidth: true
                implicitHeight: 30
                leftPadding: 4 + modelData.depth * 14
                highlighted: root.navigationController.currentPath === modelData.path
                // The keyboard's row, drawn as focus; the browsed folder is
                // highlighted, so neither is shown by colour alone.
                background: Rectangle {
                    radius: 4
                    color: rowItem.highlighted ? rowItem.palette.alternateBase : "transparent"
                    border.width: root.activeFocus && root.currentRow === rowItem.index ? 2 : 0
                    border.color: rowItem.palette.highlight
                }
                contentItem: RowLayout {
                    spacing: 2
                    ToolButton {
                        objectName: "folderTreeToggle_" + rowItem.modelData.path
                        implicitWidth: 22
                        implicitHeight: 22
                        padding: 2
                        flat: true
                        focusPolicy: Qt.NoFocus
                        text: rowItem.modelData.open ? "▾" : "▸"
                        Accessible.name: rowItem.modelData.open
                            ? qsTr("Hide folders in %1").arg(rowItem.modelData.name)
                            : qsTr("Show folders in %1").arg(rowItem.modelData.name)
                        onClicked: root.setOpen(rowItem.index, !rowItem.modelData.open)
                    }
                    Image {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        source: "image://theme-icons/"
                            + (rowItem.modelData.depth === 0 && rowItem.modelData.path !== "/"
                               ? "user-home" : rowItem.modelData.path === "/" ? "drive-harddisk" : "folder")
                        sourceSize: Qt.size(18, 18)
                        Accessible.ignored: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: rowItem.modelData.name
                        font.bold: rowItem.highlighted
                        elide: Text.ElideRight
                        Accessible.ignored: true
                    }
                }
                focusPolicy: Qt.NoFocus
                Accessible.role: Accessible.TreeItem
                Accessible.name: modelData.name
                Accessible.description: (modelData.open ? qsTr("Open, %1") : qsTr("Closed, %1"))
                    .arg(modelData.path)
                Accessible.selected: highlighted
                onClicked: {
                    root.forceActiveFocus()
                    root.openRow(rowItem.index)
                }
            }
        }
    }
}
