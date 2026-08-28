// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Item {
    id: root

    required property var navigationController

    function activateCurrent() {
        if (listView.currentIndex >= 0) {
            root.navigationController.activate(listView.currentIndex)
        }
    }

    // AGENT-GUARD: Preserve the previously selected entry's name across a
    // refresh of the same folder so review-visible selection stays
    // deterministic instead of silently jumping to index 0 whenever the
    // underlying listing is rebuilt. A genuine navigation to a different
    // folder still lands on index 0 because the old name normally will not
    // exist there.
    property string lastSelectedName: ""

    Connections {
        target: root.navigationController
        function onEntriesChanged() {
            const restored = root.navigationController.indexOfName(root.lastSelectedName)
            listView.currentIndex = restored >= 0
                ? restored
                : (root.navigationController.entries.length > 0 ? 0 : -1)
        }
    }

    // AGENT-NOTE: The controller publishes a non-empty statusMessage only for
    // a truncated ready-state listing (errors use StatePane instead), so this
    // notice is the single user-visible surface for that bound. Keep it
    // outside the ListView so the notice cannot scroll out of view.
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        spacing: 0

        ListView {
            id: listView
            objectName: "entryListView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            focus: true
            keyNavigationEnabled: true
            model: root.navigationController.entries

            Accessible.role: Accessible.List
            Accessible.name: qsTr("Folder contents")

            onCurrentIndexChanged: {
                root.lastSelectedName = currentIndex >= 0
                    ? root.navigationController.entries[currentIndex].name : ""
            }

            Keys.onReturnPressed: root.activateCurrent()
            Keys.onEnterPressed: root.activateCurrent()
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Backspace) {
                    root.navigationController.goUp()
                    event.accepted = true
                }
            }

            delegate: Rectangle {
                id: delegateRoot

                required property var modelData
                required property int index

                width: listView.width
                height: 36
                color: ListView.isCurrentItem ? Tokens.state.pressed
                     : hoverArea.containsMouse ? Tokens.state.hover : "transparent"

                Accessible.role: Accessible.ListItem
                Accessible.name: delegateRoot.modelData.name + (delegateRoot.modelData.isDirectory
                    ? qsTr(", folder") : qsTr(", file"))
                Accessible.selected: ListView.isCurrentItem

                Qinda.Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Tokens.space["3"]
                    anchors.rightMargin: Tokens.space["3"]
                    text: delegateRoot.modelData.name
                    muted: delegateRoot.modelData.isHidden
                    elide: Text.ElideMiddle
                    Accessible.ignored: true
                }

                MouseArea {
                    id: hoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: listView.currentIndex = delegateRoot.index
                    onDoubleClicked: {
                        listView.currentIndex = delegateRoot.index
                        root.activateCurrent()
                    }
                }
            }
        }

        Qinda.Label {
            objectName: "truncationNotice"
            Layout.fillWidth: true
            Layout.topMargin: Tokens.space["1"]
            visible: root.navigationController.statusMessage.length > 0
            text: root.navigationController.statusMessage
            muted: true
        }
    }
}
