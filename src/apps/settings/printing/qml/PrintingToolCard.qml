// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root
    required property var toolRow
    required property bool busy
    property Item nextFocusTarget: null
    property Item previousFocusTarget: null
    readonly property alias actionButton: launchButton
    signal launchRequested(string toolId)
    signal focusEntered()
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: root.toolRow.title
    }
    Label {
        Layout.fillWidth: true
        text: root.toolRow.description
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
    }
    Label {
        objectName: root.toolRow.id + "Availability"
        Layout.fillWidth: true
        text: root.toolRow.reason.length > 0
              ? root.toolRow.reason
              : qsTr("Installed tool: %1").arg(root.toolRow.applicationName)
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        muted: true
        Accessible.name: text
    }
    Button {
        id: launchButton
        objectName: root.toolRow.id + "LaunchButton"
        Layout.fillWidth: true
        text: root.toolRow.actionText
        available: root.toolRow.available
        busy: root.busy
        accessibleDescription: root.toolRow.description
        KeyNavigation.tab: root.nextFocusTarget
        KeyNavigation.backtab: root.previousFocusTarget
        onActiveFocusChanged: if (activeFocus) root.focusEntered()
        // Enter has the same deliberate intent as Space, independent of the
        // platform's default-button convention. The model still admits it.
        Keys.onReturnPressed: event => {
            if (enabled) root.launchRequested(root.toolRow.id)
            event.accepted = true
        }
        Keys.onEnterPressed: event => {
            if (enabled) root.launchRequested(root.toolRow.id)
            event.accepted = true
        }
        onClicked: root.launchRequested(root.toolRow.id)
    }
}
