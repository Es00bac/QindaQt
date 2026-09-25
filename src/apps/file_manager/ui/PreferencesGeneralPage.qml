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

        // ADR-0271: the File manager style. Picking one also sets "Open
        // folders as" to the style's starting view; "Match the desktop
        // layout" follows the layout chosen in Settings.
        Label { text: qsTr("File manager style"); Accessible.ignored: true }
        ComboBox {
            objectName: "preferenceStyleBox"
            Layout.fillWidth: true
            readonly property var names: ({ "finder": qsTr("Finder"), "explorer": qsTr("Explorer"),
                                            "commander": qsTr("Commander") })
            readonly property var choices: [""].concat(root.preferencesController.fileManagerStyles)
            textRole: "text"
            valueRole: "value"
            model: choices.map(style => ({ "value": style, "text": style.length > 0 ? names[style]
                : qsTr("Match the desktop layout (%1)").arg(names[root.preferencesController.layoutStyleHint]) }))
            currentIndex: choices.indexOf(root.preferencesController.fileManagerStyleChoice)
            Accessible.name: qsTr("File manager style")
            onActivated: root.preferencesController.setFileManagerStyle(currentValue)
        }

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

    Label {
        Layout.fillWidth: true
        text: qsTr("Finder: places and views, as always. Explorer: a folder tree, an address "
                 + "bar and a command bar, in Details. Commander: two folders side by side, "
                 + "Tab between them, function keys along the bottom.")
        color: root.palette.placeholderText
        wrapMode: Text.WordWrap
        Accessible.ignored: true
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
