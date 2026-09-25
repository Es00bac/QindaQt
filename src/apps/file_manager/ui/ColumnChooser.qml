// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaTK as Tk

// The Details view's column chooser (ADR-0270), opened by right-clicking the
// table header or with View ▸ Show Columns: which columns show and in what
// order, Group By, and the Details presentation preferences, plus Use as
// Defaults. Presentation only: every control writes through the owner of its
// value (the column set, NavigationController, PreferencesController,
// FolderViewSettings) and shows what that owner publishes.
Tk.Popover {
    id: root
    objectName: "columnChooser"

    required property var view
    required property var columnSet
    required property var navigationController
    property var preferencesController: null
    property var folderViews: null

    readonly property var groupKeys: ["none", "kind", "date", "size"]
    // Shown columns in their order, then the others; Name never moves.
    readonly property var rows: {
        const shown = root.columnSet.folderColumns.map(column => column.key)
            .filter(key => key !== "name")
        const hidden = root.columnSet.keys.filter(key => key !== "name" && shown.indexOf(key) < 0)
        return shown.concat(hidden)
    }
    readonly property int shownCount: root.columnSet.folderColumns.length - 1

    placement: "bottom"
    padding: Tk.Theme.space.md

    contentItem: ColumnLayout {
        spacing: Tk.Theme.space.sm

        Tk.Overline { text: qsTr("Columns") }

        Repeater {
            model: root.rows
            delegate: RowLayout {
                id: chooserRow
                required property var modelData
                required property int index
                readonly property string key: String(modelData)
                readonly property bool shown: chooserRow.index < root.shownCount
                spacing: Tk.Theme.space.xs

                Tk.CheckBox {
                    objectName: "columnToggle_" + chooserRow.key
                    Layout.fillWidth: true
                    Layout.minimumWidth: 160
                    text: root.columnSet.titleOf(chooserRow.key)
                    checked: chooserRow.shown
                    onToggled: root.columnSet.showColumn(chooserRow.key, checked)
                }
                Tk.IconButton {
                    objectName: "columnEarlier_" + chooserRow.key
                    iconName: "chevron-up"
                    small: true
                    tooltip: qsTr("Move %1 earlier").arg(root.columnSet.titleOf(chooserRow.key))
                    enabled: chooserRow.shown && chooserRow.index > 0
                    onClicked: root.columnSet.moveColumn(chooserRow.key, -1)
                }
                Tk.IconButton {
                    objectName: "columnLater_" + chooserRow.key
                    iconName: "chevron-down"
                    small: true
                    tooltip: qsTr("Move %1 later").arg(root.columnSet.titleOf(chooserRow.key))
                    enabled: chooserRow.shown && chooserRow.index < root.shownCount - 1
                    onClicked: root.columnSet.moveColumn(chooserRow.key, 1)
                }
            }
        }

        Tk.Divider { Layout.fillWidth: true }

        RowLayout {
            spacing: Tk.Theme.space.md
            Tk.Label { text: qsTr("Group by"); Accessible.ignored: true }
            Tk.ComboBox {
                objectName: "groupByBox"
                Layout.fillWidth: true
                // ADR-0262: Applications groups by category instead.
                enabled: !root.view.applicationsPlace
                tooltip: qsTr("Group by")
                model: [qsTr("None"), qsTr("Kind"), qsTr("Date Modified"), qsTr("Size")]
                currentIndex: Math.max(0, root.groupKeys.indexOf(root.navigationController.groupBy))
                onActivated: (index) => root.navigationController.setGroupBy(root.groupKeys[index])
            }
        }

        // Window-wide Details preferences (ADR-0198's store), shown only
        // when there is a store to write them to.
        ColumnLayout {
            visible: root.preferencesController !== null
            spacing: Tk.Theme.space.xs
            Tk.Switch {
                objectName: "foldersFirstSwitch"
                text: qsTr("Folders first")
                checked: root.preferencesController !== null
                    && root.preferencesController.directoriesFirst === true
                onToggled: root.preferencesController.setDirectoriesFirst(checked)
            }
            Tk.Switch {
                objectName: "relativeDatesSwitch"
                text: qsTr("Relative dates")
                checked: root.preferencesController !== null
                    && root.preferencesController.relativeDates === true
                onToggled: root.preferencesController.setRelativeDates(checked)
            }
            Tk.Switch {
                objectName: "compactRowsSwitch"
                text: qsTr("Compact rows")
                checked: root.preferencesController !== null
                    && root.preferencesController.rowDensity === "compact"
                onToggled: root.preferencesController.setRowDensity(checked ? "compact" : "comfortable")
            }
            Tk.Switch {
                objectName: "showExtensionsSwitch"
                text: qsTr("Show filename extensions")
                checked: root.preferencesController !== null
                    && root.preferencesController.showExtensions === true
                onToggled: root.preferencesController.setShowExtensions(checked)
            }
        }

        Tk.Button {
            objectName: "useAsDefaultsButton"
            Layout.alignment: Qt.AlignRight
            visible: root.folderViews !== null
            text: qsTr("Use as Defaults")
            tooltip: qsTr("Make this folder's view the one every other folder starts with")
            onClicked: root.folderViews.useAsDefaults()
        }
    }
}
