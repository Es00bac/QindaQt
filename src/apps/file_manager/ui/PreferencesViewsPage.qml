// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The default folder view's order, grouping and icon size (ADR-0198,
// ADR-0270), and the window-wide Details presentation. A folder the user gave
// a view of its own keeps it; View ▸ Use as Defaults makes one the default.
ColumnLayout {
    id: root
    objectName: "preferencesViewsPage"

    required property var preferencesController

    spacing: 10

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 12
        rowSpacing: 8

        Label { text: qsTr("Sort by"); Accessible.ignored: true }
        ComboBox {
            objectName: "preferenceSortColumnBox"
            Layout.fillWidth: true
            readonly property var names: ({
                "name": qsTr("Name"), "size": qsTr("Size"), "kind": qsTr("Kind"),
                "modified": qsTr("Date Modified"), "created": qsTr("Date Created"),
                "accessed": qsTr("Date Accessed"), "extension": qsTr("Extension"),
                "path": qsTr("Path"), "permissions": qsTr("Permissions") })
            textRole: "text"
            valueRole: "value"
            model: root.preferencesController.sortColumns.map(key => ({ "value": key, "text": names[key] || key }))
            currentIndex: root.preferencesController.sortColumns.indexOf(root.preferencesController.sortColumn)
            Accessible.name: qsTr("Sort folders by")
            onActivated: root.preferencesController.setSortColumn(currentValue)
        }

        Label { text: qsTr("Order"); Accessible.ignored: true }
        ComboBox {
            objectName: "preferenceSortDirectionBox"
            Layout.fillWidth: true
            model: root.preferencesController.sortDirections
            currentIndex: model.indexOf(root.preferencesController.sortDirection)
            Accessible.name: qsTr("Sort order")
            onActivated: root.preferencesController.setSortDirection(currentText)
        }

        Label { text: qsTr("Group by"); Accessible.ignored: true }
        ComboBox {
            objectName: "preferenceGroupByBox"
            Layout.fillWidth: true
            readonly property var names: ({ "none": qsTr("None"), "kind": qsTr("Kind"),
                                            "date": qsTr("Date Modified"), "size": qsTr("Size") })
            textRole: "text"
            valueRole: "value"
            model: root.preferencesController.groupKeys.map(key => ({ "value": key, "text": names[key] || key }))
            currentIndex: root.preferencesController.groupKeys.indexOf(root.preferencesController.groupBy)
            Accessible.name: qsTr("Group folders by")
            onActivated: root.preferencesController.setGroupBy(currentValue)
        }

        Label { text: qsTr("Icon size"); Accessible.ignored: true }
        ComboBox {
            objectName: "preferenceIconSizeBox"
            Layout.fillWidth: true
            model: root.preferencesController.iconSizes
            currentIndex: model.indexOf(root.preferencesController.iconSize)
            Accessible.name: qsTr("Icon size in pixels")
            onActivated: root.preferencesController.setIconSize(Number(currentText))
        }
    }

    CheckBox {
        objectName: "preferenceDirectoriesFirstBox"
        text: qsTr("List folders before files")
        checked: root.preferencesController.directoriesFirst
        Accessible.name: text
        onToggled: root.preferencesController.setDirectoriesFirst(checked)
    }

    CheckBox {
        objectName: "preferenceRelativeDatesBox"
        text: qsTr("Show recent dates as Today and Yesterday")
        checked: root.preferencesController.relativeDates
        Accessible.name: text
        onToggled: root.preferencesController.setRelativeDates(checked)
    }

    CheckBox {
        objectName: "preferenceCompactRowsBox"
        text: qsTr("Compact rows in the Details view")
        checked: root.preferencesController.rowDensity === "compact"
        Accessible.name: text
        onToggled: root.preferencesController.setRowDensity(checked ? "compact" : "comfortable")
    }

    CheckBox {
        objectName: "preferenceShowExtensionsBox"
        text: qsTr("Show filename extensions")
        checked: root.preferencesController.showExtensions
        Accessible.name: text
        onToggled: root.preferencesController.setShowExtensions(checked)
    }

    Item { Layout.fillHeight: true }
}
