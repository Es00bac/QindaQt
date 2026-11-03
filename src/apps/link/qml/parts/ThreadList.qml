// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import QindaTK as Tk
import QindaQt.Link

// Conversation list: search over the loaded threads, unread badges, paging at the bottom.
Tk.Flex {
    id: list
    objectName: "threadList"
    direction: Tk.Flex.Column
    gap: 0

    Tk.Box {
        Tk.Flex.shrink: 0
        padding: Tk.Theme.space.sm
        borderBottom: 1
        borderColor: Tk.Theme.color.divider
        Tk.SearchField {
            objectName: "threadSearch"
            small: true
            placeholderText: qsTr("Search conversations")
            tooltip: qsTr("Matches names, numbers and the latest message of the conversations loaded so far")
            onTextChanged: Threads.searchText = text
        }
    }

    Item {
        Tk.Flex.grow: 1
        Tk.Flex.basis: 0
        Tk.Flex.minHeight: 0

        ListView {
            id: view
            objectName: "threadView"
            anchors.fill: parent
            clip: true
            model: Threads.threads
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: Tk.ScrollBar {}
            onAtYEndChanged: if (atYEnd && Threads.hasMore) Threads.loadMore()

            delegate: Tk.ListRow {
                id: row
                required property int index
                required property real threadId
                required property string title
                required property string snippet
                required property string when
                required property int unread
                width: ListView.view.width
                text: title
                secondaryText: snippet
                selected: Conversation.threadId === threadId
                tooltip: title
                onClicked: Conversation.openThread(Threads.threads.get(index))
                trailing: Tk.Flex {
                    direction: Tk.Flex.Column
                    align: Tk.Flex.End
                    gap: 0
                    Tk.Caption { text: row.when }
                    Tk.Badge {
                        visible: row.unread > 0
                        variant: "accent"
                        text: String(row.unread)
                    }
                }
            }
        }

        Tk.EmptyState {
            anchors.centerIn: parent
            visible: Threads.threads.count === 0 && !Threads.loading
            iconName: Threads.error.length > 0 ? "triangle-alert" : "inbox"
            title: Threads.error.length > 0 ? qsTr("Could not load messages")
                 : Threads.searchText.length > 0 ? qsTr("No match") : qsTr("No conversations")
            text: Threads.error.length > 0 ? Threads.error
                 : Threads.searchText.length > 0 ? qsTr("Nothing loaded matches the search.") : qsTr("Messages on the phone appear here.")
        }
    }

    Tk.Box {
        Tk.Flex.shrink: 0
        visible: Threads.hasMore || Threads.loading
        implicitHeight: Tk.Theme.size.row
        borderTop: 1
        borderColor: Tk.Theme.color.divider
        Tk.Caption {
            anchors.centerIn: parent
            text: Threads.loading ? qsTr("Loading...") : qsTr("%1 conversations loaded").arg(Threads.loadedCount)
        }
    }
}
