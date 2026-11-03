// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "LinkText.js" as LinkText

// Setup > Connection: how this phone reaches the desktop without a cable. Wi-Fi status, desktops found by
// mDNS (only while not connected), a manual address (Tailscale IP or MagicDNS name) and the Wi-Fi pairing
// hint. AGENT-CONTRACT: `link.lan` = {manual, manualError, discovering, selected, desktops:[{id,name}]}
// (LinkBridge::lan / FakeBridge); discovered addresses never reach QML.
Tk.Flex {
    id: root
    objectName: "connectionSection"
    direction: Tk.Flex.Column
    gap: Tk.Theme.space.md

    property bool pairHint: false
    readonly property var desktops: link.lan.desktops === undefined ? [] : link.lan.desktops
    readonly property string selected: link.lan.selected === undefined ? "" : link.lan.selected

    function networkText(n) {
        return n === "wifi" ? qsTr("Wi-Fi") : n === "cellular" ? qsTr("Mobile data") : n === "none" ? qsTr("Offline") : qsTr("Other")
    }

    Tk.SectionHeader { title: qsTr("Connection") }

    Tk.Card {
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm
            Tk.ListRow {
                objectName: "wifiRow"
                iconName: "link"
                text: qsTr("Phone network")
                secondaryText: root.networkText(link.network)
            }
            Tk.ListRow {
                objectName: "linkRow"
                iconName: "link"
                text: qsTr("Linked over")
                secondaryText: LinkText.transportLabel(link.transport)
            }
            Tk.Label {
                text: qsTr("Desktops on this network")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Repeater {
                model: root.desktops
                delegate: Tk.ListRow {
                    required property var modelData
                    objectName: "desktop_" + modelData.id
                    iconName: "monitor"
                    text: modelData.name
                    selected: modelData.id === root.selected
                    tooltip: qsTr("Choose this desktop to pair with")
                    Tk.Flex.alignSelf: Tk.Flex.Stretch
                    onClicked: link.selectDesktop(modelData.id === root.selected ? "" : modelData.id)
                }
            }
            Tk.Caption {
                objectName: "discoveryHint"
                visible: root.desktops.length === 0
                muted: true
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                text: link.lan.discovering ? qsTr("Searching. Turn on Wi-Fi link in QindaLink on the desktop.")
                    : LinkText.connected(link.phase) ? qsTr("Searching pauses while the desktop is linked.")
                    : qsTr("Not searching right now.")
            }
        }
    }

    Tk.Card {
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm
            Tk.Label { text: qsTr("Desktop address"); Tk.Flex.alignSelf: Tk.Flex.Stretch }
            Tk.Caption {
                muted: true
                wrapMode: Text.Wrap
                text: qsTr("For Tailscale or another network: the desktop's IP address or name, optionally with :port.")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.TextField {
                id: field
                objectName: "manualAddress"
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                placeholderText: qsTr("100.x.y.z or name.ts.net:47160")
                tooltip: qsTr("Desktop address for connections outside the home network")
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly
                error: link.lan.manualError !== undefined && link.lan.manualError.length > 0
                onAccepted: saveButton.clicked()
                // Show the saved value unless the user is typing.
                Component.onCompleted: text = link.lan.manual === undefined ? "" : link.lan.manual
            }
            Tk.Caption {
                objectName: "manualError"
                visible: link.lan.manualError !== undefined && link.lan.manualError.length > 0
                text: link.lan.manualError === undefined ? "" : link.lan.manualError
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Button {
                id: saveButton
                objectName: "saveAddress"
                text: qsTr("Save address")
                variant: "outline"
                tooltip: qsTr("Remember this address and try it when USB and Wi-Fi discovery fail")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                onClicked: link.setManualAddress(field.text)
            }
        }
    }

    Tk.Button {
        objectName: "pairOverWifi"
        visible: !link.paired
        large: true
        variant: "accent"
        text: qsTr("Pair over Wi-Fi")
        tooltip: qsTr("Pair with the chosen desktop or the saved address")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: root.pairHint = true
    }
    Tk.Notice {
        objectName: "pairHint"
        visible: root.pairHint && !link.paired
        variant: "info"
        title: qsTr("Open the pairing window on the desktop")
        text: qsTr("On the desktop, open QindaLink and choose Allow Wi-Fi pairing. You have two minutes. Then come back here: when the desktop shows up, tap Pair with this desktop on Home and compare the codes.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
}
