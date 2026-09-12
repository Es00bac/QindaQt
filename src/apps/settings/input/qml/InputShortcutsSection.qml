// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Every global shortcut the desktop authority reports, searchable, with
// capture-to-assign, conflict naming, reset, clear, and custom command
// shortcuts. Conflicts are computed by the model and always name the
// conflicting action (ADR-0134).
ColumnLayout {
    id: root

    required property var inputSettings
    readonly property var shortcuts: inputSettings.shortcuts

    readonly property Item firstFocusTarget: degraded.visible ? degraded
                                             : searchField

    Component.onCompleted: shortcuts.refresh()

    spacing: Tokens.space["2"]

    DegradedNotice {
        id: degraded
        objectName: "inputShortcutsDegraded"
        Layout.fillWidth: true
        visible: !shortcuts.available
        reason: qsTr("Global shortcuts are unavailable right now.")
    }

    FormRow {
        objectName: "inputShortcutsSearchRow"
        Layout.fillWidth: true
        visible: shortcuts.available
        label: qsTr("Search")
        description: qsTr("Filter by component or action name")
        editor: TextField {
            id: searchField
            objectName: "inputShortcutsSearchField"
            placeholderText: qsTr("Type to filter")
            width: 320
            onTextChanged: shortcuts.filter = text
        }
    }

    Label {
        objectName: "inputShortcutsStatus"
        Layout.fillWidth: true
        visible: text.length > 0
        text: shortcuts.statusText
        wrapMode: Text.Wrap
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    // AGENT-NOTE: The page's Flickable scrolls this section and a ListView
    // reports no implicit height, so a fill-height list collapses to nothing
    // there. The list takes its content height and the page scrolls it, the
    // same contract as the keyboard layouts list.
    ListView {
        id: shortcutList
        objectName: "inputShortcutsList"
        Layout.fillWidth: true
        Layout.preferredHeight: contentHeight
        visible: shortcuts.available
        interactive: false
        clip: true
        model: shortcuts
        spacing: Tokens.space["1"]
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Global shortcuts")

        delegate: InputShortcutRow {
            shortcuts: root.shortcuts
        }
    }

    SectionHeader {
        objectName: "inputCustomCommandsHeader"
        Layout.fillWidth: true
        visible: shortcuts.available
        title: qsTr("Custom command shortcut")
    }

    RowLayout {
        objectName: "inputCustomCommandRow"
        Layout.fillWidth: true
        visible: shortcuts.available
        spacing: Tokens.space["2"]

        TextField {
            id: commandName
            objectName: "inputCommandNameField"
            placeholderText: qsTr("Name")
            Layout.preferredWidth: 160
            Accessible.name: qsTr("Command shortcut name")
        }
        TextField {
            id: commandLine
            objectName: "inputCommandLineField"
            placeholderText: qsTr("Command to run")
            Layout.preferredWidth: 260
            Accessible.name: qsTr("Command to run")
        }
        ShortcutCaptureButton {
            id: commandCapture
            objectName: "inputCommandCapture"
            placeholderText: qsTr("Set shortcut")
            formatter: key => shortcuts.displayKey(key)
        }
        Button {
            objectName: "inputCommandAddButton"
            emphasized: true
            text: qsTr("Add")
            enabled: commandName.text.trim().length > 0
                     && commandLine.text.trim().length > 0
            onClicked: {
                if (shortcuts.addCommand(commandName.text,
                                         commandLine.text,
                                         commandCapture.sequence !== 0
                                             ? [commandCapture.sequence]
                                             : [])) {
                    commandName.text = ""
                    commandLine.text = ""
                    commandCapture.sequence = 0
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}
