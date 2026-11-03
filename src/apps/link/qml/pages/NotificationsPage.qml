// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import QindaTK as Tk
import QindaQt.Link
import "../parts" as Parts

// Notifications: which phone apps may show notifications on this desktop. A muted app's
// notifications are closed here and never shown again until unmuted.
Tk.Flex {
    id: page
    objectName: "notificationsPage"
    direction: Tk.Flex.Column
    gap: 0

    Component.onCompleted: Mutes.refresh()

    Parts.ServiceNotice { Tk.Flex.shrink: 0 }

    Tk.Box {
        Tk.Flex.shrink: 0
        padding: Tk.Theme.space.sm
        color: Tk.Theme.color.headerBg
        borderBottom: 1
        borderColor: Tk.Theme.color.divider
        Tk.Flex {
            align: Tk.Flex.Center
            gap: Tk.Theme.space.sm
            Tk.SearchField {
                objectName: "muteSearch"
                small: true
                implicitWidth: 240
                placeholderText: qsTr("Search apps")
                onTextChanged: Mutes.searchText = text
            }
            Tk.Caption {
                visible: Mutes.loaded
                text: qsTr("%1 of %2 muted").arg(Mutes.mutedCount).arg(Mutes.totalCount)
            }
        }
    }

    Item {
        Tk.Flex.grow: 1
        Tk.Flex.basis: 0
        Tk.Flex.minHeight: 0

        ListView {
            id: view
            objectName: "muteView"
            anchors.fill: parent
            clip: true
            model: Mutes.apps
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: Tk.ScrollBar {}

            delegate: Tk.ListRow {
                id: row
                required property string pkg
                required property string label
                required property bool muted
                width: ListView.view.width
                text: label
                secondaryText: pkg
                iconName: muted ? "eye-off" : "bell"
                trailing: Tk.Switch {
                    checked: !row.muted
                    small: true
                    tooltip: row.muted ? qsTr("Muted: turn on to show notifications from %1").arg(row.label)
                                       : qsTr("Showing notifications from %1: turn off to mute").arg(row.label)
                    onToggled: Mutes.setMuted(row.pkg, !checked)
                }
            }
        }

        Tk.EmptyState {
            anchors.centerIn: parent
            width: Math.min(parent.width - 32, 360)
            maxTextWidth: width
            visible: Mutes.apps.count === 0
            iconName: Mutes.error.length > 0 ? "triangle-alert" : "bell"
            title: Mutes.error.length > 0 ? qsTr("Could not load apps") : Mutes.loaded ? qsTr("No apps yet") : qsTr("Loading")
            text: Mutes.error.length > 0 ? Mutes.error
                  : qsTr("Apps appear here after they post a notification on the phone, or once you mute them.")
        }
    }
}
