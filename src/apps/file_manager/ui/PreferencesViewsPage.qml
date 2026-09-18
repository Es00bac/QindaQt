// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Sorting and icon size (ADR-0198). These used to be session-local, which is
// the ADR-0090 deferral this page closes.
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
            model: root.preferencesController.sortColumns
            currentIndex: model.indexOf(root.preferencesController.sortColumn)
            Accessible.name: qsTr("Sort folders by")
            onActivated: root.preferencesController.setSortColumn(currentText)
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

    Item { Layout.fillHeight: true }
}
