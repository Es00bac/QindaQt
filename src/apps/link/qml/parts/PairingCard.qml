// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// One pairing request. The code is what both screens must show; Accept is a deliberate click and
// Reject takes the focus, so a stray Enter refuses rather than trusts.
Tk.Box {
    id: card

    property string deviceName: ""
    property string deviceModel: ""
    property string pairingCode: ""
    signal accepted()
    signal rejected()

    objectName: "pairingCard"
    color: Tk.Theme.color.accentSubtle
    borderWidth: 1
    borderColor: Tk.Theme.color.accent
    radius: Tk.Theme.radius.md
    padding: Tk.Theme.space.md

    Tk.Flex {
        direction: Tk.Flex.Row
        align: Tk.Flex.Center
        gap: Tk.Theme.space.lg
        wrap: Tk.Flex.Wrap

        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.xs
            Tk.Flex.grow: 1
            Tk.Flex.minWidth: 180
            Tk.Overline { title: qsTr("Pairing request") }
            Tk.Label { text: card.deviceName + (card.deviceModel.length > 0 ? " (" + card.deviceModel + ")" : "") }
            Tk.Caption { text: qsTr("Accept only if this code is the one on your phone.") }
        }
        Tk.Mono {
            objectName: "pairingCode"
            text: card.pairingCode
            font.pixelSize: Tk.Theme.font.large * 1.6
            font.weight: Font.DemiBold
            Tk.Flex.shrink: 0
        }
        Tk.Flex {
            gap: Tk.Theme.space.xs
            Tk.Flex.shrink: 0
            Tk.Button {
                id: reject
                objectName: "rejectPairing"
                text: qsTr("Reject")
                onClicked: card.rejected()
                Component.onCompleted: reject.forceActiveFocus()
            }
            Tk.Button {
                objectName: "acceptPairing"
                text: qsTr("Accept")
                variant: "accent"
                onClicked: card.accepted()
            }
        }
    }
}
