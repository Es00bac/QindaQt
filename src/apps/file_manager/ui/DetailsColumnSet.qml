// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "EntryText.js" as EntryText

// The Details view's columns (ADR-0270): one Tk.TableColumn per column key,
// declared once; the folder's column state ([{key, width}]) and its edits;
// ordered(), which turns that state into the list the table shows; and each
// cell's text.
//
// AGENT-CONTRACT: `keys` is Preferences::columnKeys() (preferences.cpp); the
// store refuses any other key, so a column added here needs its key there.
// Name is always first and always shown (DetailsNameCell carries the row).
// Columns whose values are read lazily for visible rows (owner, group,
// items, dimensions) are not sortable: sorting needs every row's value.
QtObject {
    id: root

    // The DetailsView: entryAt(), its injected owners and its presentation.
    required property var view

    // Edits land in the folder's view (FolderViewSettings); a view without
    // one (a fixture) keeps its own state.
    property var localState: [{ "key": "name", "width": 0 }, { "key": "size", "width": 0 },
                              { "key": "kind", "width": 0 }, { "key": "modified", "width": 0 }]
    readonly property var folderColumns: root.view.folderViews && root.view.folderViews.columns.length > 0
        ? root.view.folderViews.columns : root.localState
    // AGENT-GUARD: the table's column list depends on this string, not on
    // folderColumns, so a width change (every step of a seam drag) never
    // rebuilds the header cell being dragged.
    readonly property string shownKeys: root.folderColumns.map(column => column.key).join(",")
    readonly property var widthMap: {
        const map = ({})
        for (const column of root.folderColumns) {
            if (column.width > 0)
                map[column.key] = column.width
        }
        return map
    }

    function edit(next) {
        if (root.view.folderViews)
            root.view.folderViews.setColumns(next)
        else
            root.localState = next
    }
    function isShown(key) {
        return root.folderColumns.some(column => column.key === key)
    }
    function showColumn(key, shown) {
        const next = root.folderColumns.slice()
        const at = next.findIndex(column => column.key === key)
        if (key === "name" || shown === (at >= 0))
            return
        if (shown)
            next.push({ "key": key, "width": 0 })
        else
            next.splice(at, 1)
        root.edit(next)
    }
    // Name stays first: nothing moves before it, and it never moves.
    function moveColumn(key, delta) {
        const next = root.folderColumns.slice()
        const at = next.findIndex(column => column.key === key)
        const to = at + delta
        if (at <= 0 || to <= 0 || to >= next.length)
            return
        next.splice(to, 0, next.splice(at, 1)[0])
        root.edit(next)
    }
    function resizeColumn(key, width) {
        const bounded = Math.max(40, Math.min(1000, Math.round(width)))
        root.edit(root.folderColumns.map(column => column.key === key
            ? { "key": column.key, "width": bounded } : column))
    }

    // A cell's text, "—" when unknown. Reading the facts' revision makes a
    // cell re-ask once a lazily read value lands (EntryFacts).
    function cellText(key, entry) {
        const facts = root.view.entryFacts
        const revision = facts ? facts.revision : 0
        const relative = root.view.relativeDates
        const stamp = String(entry.modifiedNanoseconds || "")
        let text = ""
        if (key === "size") text = entry.sizeText || ""
        else if (key === "kind") text = entry.kindText || ""
        else if (key === "modified") text = EntryText.dateText(entry.modified, relative, root.view.locale)
        else if (key === "created") text = EntryText.dateText(entry.created, relative, root.view.locale)
        else if (key === "accessed") text = EntryText.dateText(entry.accessed, relative, root.view.locale)
        else if (key === "permissions") text = EntryText.permissionsText(entry)
        else if (key === "owner") text = facts ? facts.ownerName(Number(entry.ownerId ?? -1)) : ""
        else if (key === "group") text = facts ? facts.groupName(Number(entry.groupId ?? -1)) : ""
        else if (key === "extension") text = EntryText.extensionOf(entry)
        else if (key === "path") text = EntryText.folderOf(entry)
        else if (key === "items" && facts && entry.isDirectory === true)
            text = facts.itemCount(entry.path, stamp)
        else if (key === "dimensions" && facts && entry.iconName === "image-x-generic"
                 && entry.isSymlink !== true)
            text = facts.dimensions(entry.path, stamp)
        return revision >= 0 && text.length > 0 ? text : "—"
    }

    readonly property var keys: ["name", "size", "kind", "modified", "created", "accessed",
        "permissions", "owner", "group", "extension", "path", "items", "dimensions"]
    readonly property var byKey: ({
        "name": nameColumn, "size": sizeColumn, "kind": kindColumn,
        "modified": modifiedColumn, "created": createdColumn, "accessed": accessedColumn,
        "permissions": permissionsColumn, "owner": ownerColumn, "group": groupColumn,
        "extension": extensionColumn, "path": pathColumn, "items": itemsColumn,
        "dimensions": dimensionsColumn })

    function titleOf(key) {
        const column = root.byKey[key]
        return column ? column.title : ""
    }

    // The table's columns for `keys` (shownKeys, Name first). While search
    // results are shown, Path joins them even when not chosen: a result's
    // folder is what tells two same-named results apart.
    function ordered(keys, searchResults) {
        const out = [root.nameColumn]
        let hasPath = false
        for (const key of keys.split(",")) {
            const table = root.byKey[key]
            if (table && key !== "name") {
                out.push(table)
                hasPath = hasPath || key === "path"
            }
        }
        if (searchResults && !hasPath)
            out.push(root.pathColumn)
        return out
    }

    // A text cell. AGENT-CONTRACT (Tk.DataTable): a cell delegate reads its
    // row and column from its parent Loader; a row is an entry index or a
    // group heading object, which draws nothing here.
    property Component textCell: Component {
        Tk.Label {
            readonly property var cellRow: parent ? parent.row : undefined
            readonly property var cellColumn: parent ? parent.column : null
            readonly property var entry: typeof cellRow === "number" ? root.view.entryAt(cellRow) : null
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: cellColumn ? cellColumn.align : Text.AlignLeft
            elide: Text.ElideRight
            muted: true
            mono: cellColumn ? cellColumn.mono : false
            text: entry !== null && cellColumn ? root.cellText(cellColumn.key, entry) : ""
            Accessible.ignored: true
        }
    }
    property Component nameCell: Component {
        DetailsNameCell { view: root.view }
    }

    property Tk.TableColumn nameColumn: Tk.TableColumn {
        key: "name"; title: qsTr("Name"); flex: 1; minWidth: 140; delegate: root.nameCell
    }
    property Tk.TableColumn sizeColumn: Tk.TableColumn {
        key: "size"; title: qsTr("Size"); width: 90; minWidth: 48
        align: Qt.AlignRight; delegate: root.textCell
    }
    property Tk.TableColumn kindColumn: Tk.TableColumn {
        key: "kind"; width: 130; minWidth: 60; delegate: root.textCell
        // ADR-0262: an application's Kind is its category.
        title: root.view.applicationsPlace ? qsTr("Category") : qsTr("Kind")
    }
    property Tk.TableColumn modifiedColumn: Tk.TableColumn {
        key: "modified"; title: qsTr("Date Modified"); width: 150; minWidth: 80
        delegate: root.textCell
    }
    property Tk.TableColumn createdColumn: Tk.TableColumn {
        key: "created"; title: qsTr("Date Created"); width: 150; minWidth: 80
        delegate: root.textCell
    }
    property Tk.TableColumn accessedColumn: Tk.TableColumn {
        key: "accessed"; title: qsTr("Date Accessed"); width: 150; minWidth: 80
        delegate: root.textCell
    }
    property Tk.TableColumn permissionsColumn: Tk.TableColumn {
        key: "permissions"; title: qsTr("Permissions"); width: 100; minWidth: 70
        mono: true; delegate: root.textCell
    }
    property Tk.TableColumn ownerColumn: Tk.TableColumn {
        key: "owner"; title: qsTr("Owner"); width: 90; minWidth: 50
        sortable: false; delegate: root.textCell
    }
    property Tk.TableColumn groupColumn: Tk.TableColumn {
        key: "group"; title: qsTr("Group"); width: 90; minWidth: 50
        sortable: false; delegate: root.textCell
    }
    property Tk.TableColumn extensionColumn: Tk.TableColumn {
        key: "extension"; title: qsTr("Extension"); width: 80; minWidth: 50
        delegate: root.textCell
    }
    property Tk.TableColumn pathColumn: Tk.TableColumn {
        key: "path"; title: qsTr("Path"); width: 200; minWidth: 80
        delegate: root.textCell
    }
    property Tk.TableColumn itemsColumn: Tk.TableColumn {
        key: "items"; title: qsTr("Items"); width: 70; minWidth: 48
        align: Qt.AlignRight; sortable: false; delegate: root.textCell
    }
    property Tk.TableColumn dimensionsColumn: Tk.TableColumn {
        key: "dimensions"; title: qsTr("Dimensions"); width: 110; minWidth: 70
        sortable: false; delegate: root.textCell
    }
}
