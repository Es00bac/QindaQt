// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What a window looks like when it opens (ADR-0198). Every control writes
// straight through PreferencesController, which persists and republishes;
// nothing here keeps its own copy of a value.
ColumnLayout {
    id: root
    objectName: "preferencesGeneralPage"

    required property var preferencesController

    spacing: 10

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 12
        rowSpacing: 8

        Label { text: qsTr("Open folders as"); Accessible.ignored: true }
        // ADR-0270: the four views by name; "list" and "grid" are the
        // historical keys of Details and Icons.
        ComboBox {
            objectName: "preferenceViewModeBox"
            Layout.fillWidth: true
            readonly property var names: ({ "grid": qsTr("Icons"), "list": qsTr("Details"),
                                            "columns": qsTr("Columns"), "gallery": qsTr("Gallery") })
            textRole: "text"
            valueRole: "value"
            model: root.preferencesController.viewModes.map(mode => ({ "value": mode, "text": names[mode] }))
            currentIndex: root.preferencesController.viewModes.indexOf(root.preferencesController.defaultViewMode)
            Accessible.name: qsTr("Open folders as")
            onActivated: root.preferencesController.setDefaultViewMode(currentValue)
        }
    }

    CheckBox {
        objectName: "preferenceShowHiddenBox"
        text: qsTr("Show hidden files")
        checked: root.preferencesController.showHidden
        Accessible.name: text
        onToggled: root.preferencesController.setShowHidden(checked)
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("These are what a new window starts with. Changing one also "
                 + "applies it to the window that is already open, except in folders "
                 + "you gave a view of their own.")
        color: root.palette.placeholderText
        wrapMode: Text.WordWrap
        Accessible.ignored: true
    }

    Item { Layout.fillHeight: true }
}
