// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "EntryDrag.js" as EntryDrag

// One pane of the window (ADR-0271): its tabs, a tab bar once it has more
// than one, and in the Commander style a header naming its folder, bold
// and in the selection colours while it is the pane the keys act on.
//
// AGENT-CONTRACT: presentation only. FolderPanes owns `tabs` (FolderTab
// items parented to `tabArea`) and `currentIndex`, and answers the three
// requests below; this pane never creates, closes or activates a tab itself.
ColumnLayout {
    id: root

    property var tabs: []
    property int currentIndex: 0
    readonly property var currentTab: root.tabs.length > 0
        ? root.tabs[Math.max(0, Math.min(root.currentIndex, root.tabs.length - 1))] : null
    readonly property var navigation: root.currentTab ? root.currentTab.navigationController : null
    // The pane the window's commands and keys act on.
    property bool active: true
    property bool showHeader: false
    property string paneName: ""
    // Drop targets for the tab buttons; null refuses drops politely.
    property var mutationController: null
    property var clipboardController: null
    readonly property alias tabArea: tabArea
    // Tabs declared inside a FolderPane (FolderPanes' first tab) land in
    // the tab area, like the ones FolderPanes creates there.
    default property alias tabContent: tabArea.data

    // A header click or a tab choice: make this the active pane.
    signal activated()
    signal newTabRequested()
    signal closeTabRequested(int index)

    spacing: 0

    Control {
        id: header
        objectName: "paneHeader"
        Layout.fillWidth: true
        visible: root.showHeader
        padding: 6
        leftPadding: 10
        background: Rectangle {
            color: root.active ? header.palette.highlight : header.palette.alternateBase
        }
        contentItem: Label {
            text: root.navigation ? root.navigation.currentPath : ""
            color: root.active ? header.palette.highlightedText : header.palette.text
            font.bold: root.active
            elide: Text.ElideMiddle
            Accessible.ignored: true
        }
        Accessible.role: Accessible.StaticText
        Accessible.name: root.active
            ? qsTr("%1, active: %2").arg(root.paneName).arg(root.navigation ? root.navigation.currentPath : "")
            : qsTr("%1: %2").arg(root.paneName).arg(root.navigation ? root.navigation.currentPath : "")
        TapHandler { onTapped: root.activated() }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.tabs.length > 1
        spacing: 0

        TabBar {
            id: tabBar
            objectName: "folderTabBar"
            Layout.fillWidth: true
            // The documented TabBar two-way pattern: a click moves the bar,
            // Ctrl+Tab moves the pane, and each follows the other.
            currentIndex: root.currentIndex
            onCurrentIndexChanged: {
                if (currentIndex >= 0 && currentIndex !== root.currentIndex) {
                    root.currentIndex = currentIndex
                    root.activated()
                }
            }
            Accessible.name: qsTr("%1 tabs").arg(root.paneName)

            Repeater {
                model: root.tabs

                TabButton {
                    id: tabButton
                    required property var modelData
                    required property int index
                    objectName: "folderTab_" + index
                    text: modelData.title
                    Accessible.description: qsTr("Show %1. Ctrl+W closes the current tab.")
                        .arg(modelData.navigationController.currentPath)

                    TapHandler {
                        acceptedButtons: Qt.MiddleButton
                        onTapped: root.closeTabRequested(tabButton.index)
                    }

                    // Drag a file onto a tab to move it (copy with Ctrl) into
                    // that tab's folder, as onto a sidebar place. Network
                    // folders are not local drop targets there either.
                    DropArea {
                        anchors.fill: parent
                        enabled: !tabButton.modelData.navigationController.remoteActive
                        onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
                        onDropped: (drop) => {
                            const action = EntryDrag.dispatch(
                                drop, tabButton.modelData.navigationController.currentPath,
                                root.mutationController, root.clipboardController)
                            if (action !== Qt.IgnoreAction)
                                drop.accept(action)
                            else
                                drop.accepted = false
                        }
                    }
                }
            }
        }

        IconButton {
            objectName: "newTabButton"
            iconName: "tab-new"
            text: qsTr("New tab (Ctrl+T)")
            implicitWidth: 32
            implicitHeight: 32
            onClicked: root.newTabRequested()
        }
    }

    // The tabs themselves, one shown at a time (FolderTab.visible). Focus
    // entering a view here makes this the active pane (FolderPanes).
    Item {
        id: tabArea
        objectName: "paneTabArea"
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}
