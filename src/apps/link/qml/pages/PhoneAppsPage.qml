// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import QindaTK as Tk
import QindaQt.Link
import "../parts" as Parts

// Phone apps: everything launchable on the phone, starred ones first. "Open" starts the app in its
// own desktop window over USB debugging (qindaqt-link-mirror, one process per window).
Tk.Flex {
    id: page
    objectName: "phoneAppsPage"
    direction: Tk.Flex.Column
    gap: 0

    Component.onCompleted: if (PhoneApps.state === "idle") PhoneApps.refresh()

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
                objectName: "appSearch"
                small: true
                implicitWidth: 240
                placeholderText: qsTr("Search apps")
                tooltip: qsTr("Match on app name or package")
                onTextChanged: PhoneApps.searchText = text
            }
            Tk.Caption {
                visible: PhoneApps.state === "ready"
                text: qsTr("%1 apps").arg(PhoneApps.totalCount)
            }
            Tk.Spacer {}
            Tk.Button {
                objectName: "refreshApps"
                text: qsTr("Refresh")
                iconName: "refresh-cw"
                small: true
                busy: PhoneApps.state === "loading"
                onClicked: PhoneApps.refresh()
            }
        }
    }

    Item {
        Tk.Flex.grow: 1
        Tk.Flex.basis: 0
        Tk.Flex.minHeight: 0

        ListView {
            id: view
            objectName: "appView"
            anchors.fill: parent
            clip: true
            visible: PhoneApps.state === "ready"
            model: PhoneApps.apps
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: Tk.ScrollBar {}

            delegate: Tk.ListRow {
                id: row
                required property string pkg
                required property string label
                required property string iconUrl
                required property bool favourite
                width: ListView.view.width
                text: label
                secondaryText: pkg
                tooltip: qsTr("Open %1 on the desktop").arg(label)
                onClicked: PhoneApps.open(pkg)
                leading: Image {
                    source: row.iconUrl
                    sourceSize.width: 48
                    sourceSize.height: 48
                    width: 24
                    height: 24
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
                trailing: Tk.Flex {
                    align: Tk.Flex.Center
                    gap: Tk.Theme.space.xs
                    Tk.IconButton {
                        iconName: "star"
                        small: true
                        checkable: true
                        checked: row.favourite
                        iconColor: row.favourite ? Tk.Theme.color.warning : Tk.Theme.color.textMuted
                        tooltip: row.favourite ? qsTr("Remove from favourites") : qsTr("Add to favourites")
                        onClicked: PhoneApps.toggleFavourite(row.pkg)
                    }
                    Tk.IconButton {
                        iconName: "external-link"
                        small: true
                        tooltip: qsTr("Open on desktop")
                        onClicked: PhoneApps.open(row.pkg)
                    }
                }
            }
        }

        Tk.EmptyState {
            anchors.centerIn: parent
            visible: PhoneApps.state !== "ready"
            objectName: "appsState"
            iconName: PhoneApps.state === "loading" ? "loader" : PhoneApps.state === "needs-adb" ? "unlink" : "layout-grid"
            title: PhoneApps.state === "loading" ? qsTr("Loading")
                 : PhoneApps.state === "needs-adb" ? qsTr("Needs USB debugging")
                 : PhoneApps.state === "error" ? qsTr("No apps") : qsTr("Phone apps")
            text: PhoneApps.message.length > 0 ? PhoneApps.message
                  : qsTr("Connect the phone to see its apps.")
        }
    }
}
