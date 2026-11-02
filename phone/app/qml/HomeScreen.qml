// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "LinkText.js" as LinkText

// Home: connection state, desktop name, this phone's battery as sent, and quick tiles.
ScreenFrame {
    id: root
    title: qsTr("QindaLink")
    subtitle: qsTr("Your phone and your QindaQt desktop")

    signal openPage(string page)

    Tk.Card {
        objectName: "connectionCard"
        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm
            Tk.Flex {
                direction: Tk.Flex.Row
                align: Tk.Flex.Center
                gap: Tk.Theme.space.sm
                Tk.Icon {
                    name: LinkText.connected(link.phase) ? "link" : "unlink"
                    size: Tk.Theme.size.iconLg
                    color: link.phase === "ready" ? Tk.Theme.color.success : Tk.Theme.color.textMuted
                }
                Tk.Heading {
                    objectName: "connectionHeadline"
                    level: 3
                    text: LinkText.headline(link.phase, link.transport)
                    wrapMode: Text.Wrap
                    Tk.Flex.grow: 1
                    Tk.Flex.basis: 0
                }
            }
            Tk.Label {
                visible: link.message.length > 0
                text: link.message
                muted: true
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.ListRow {
                objectName: "desktopRow"
                iconName: "monitor"
                text: qsTr("Desktop")
                secondaryText: link.pairedName.length > 0 ? link.pairedName
                             : link.desktopName.length > 0 ? link.desktopName : qsTr("Not connected")
            }
            Tk.ListRow {
                objectName: "transportRow"
                iconName: "link"
                text: qsTr("Connection")
                secondaryText: LinkText.transportLabel(link.transport)
            }
            Tk.ListRow {
                objectName: "batteryRow"
                iconName: "zap"
                text: qsTr("This phone's battery")
                secondaryText: LinkText.batteryText(link.batteryLevel, link.charging)
            }
        }
    }

    Tk.Button {
        objectName: "homePairButton"
        visible: link.phase === "connected" && !link.paired
        large: true
        variant: "accent"
        text: qsTr("Pair with this desktop")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: { link.pair(); root.openPage("pairing") }
    }
    Tk.Button {
        objectName: "homePairAgainButton"
        visible: link.phase === "stale"
        large: true
        variant: "accent"
        text: qsTr("Pair again")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: root.openPage("pairing")
    }

    Tk.SectionHeader { title: qsTr("Phone as a controller") }
    Tk.Grid {
        columns: "1fr 1fr"
        columnGap: Tk.Theme.space.md
        rowGap: Tk.Theme.space.md
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Repeater {
            model: [
                { page: "touchpad", title: qsTr("Touchpad"), icon: "mouse-pointer-2" },
                { page: "keyboard", title: qsTr("Keyboard"), icon: "keyboard" },
                { page: "macros", title: qsTr("Macros"), icon: "zap" },
                { page: "vault", title: qsTr("Vault"), icon: "lock" }
            ]
            delegate: Tk.Card {
                required property var modelData
                objectName: "tile_" + modelData.page
                interactive: true
                tooltip: modelData.title
                onClicked: root.openPage(modelData.page)
                Tk.Flex {
                    direction: Tk.Flex.Column
                    align: Tk.Flex.Start
                    gap: Tk.Theme.space.xs
                    Tk.Icon { name: modelData.icon; size: Tk.Theme.size.iconLg }
                    Tk.Label { text: modelData.title }
                    Tk.Caption { text: qsTr("Coming soon"); muted: true }
                }
            }
        }
    }
}
