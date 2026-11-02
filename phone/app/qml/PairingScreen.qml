// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "LinkText.js" as LinkText

// Pairing: start, compare the six-digit code, accept or reject, or renew a stale pin.
// AGENT-GUARD: Accept and Reject are the same size and both filled. Accepting a code the user has
// not compared defeats the SAS check (ADR-0434), so neither choice may look like the default.
ScreenFrame {
    id: root
    title: qsTr("Pairing")
    subtitle: link.desktopName.length > 0 ? link.desktopName : qsTr("No desktop yet")

    readonly property string phase: link.phase

    // Code comparison
    Tk.Card {
        visible: root.phase === "confirm"
        objectName: "codeCard"
        Tk.Flex {
            direction: Tk.Flex.Column
            align: Tk.Flex.Center
            gap: Tk.Theme.space.md
            padding: Tk.Theme.space.md
            Tk.Heading {
                level: 3
                text: qsTr("Pair with %1").arg(link.desktopName)
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Label {
                objectName: "pairCode"
                text: LinkText.spaced(link.code)
                font.family: Tk.Theme.font.monoFamily
                font.pixelSize: Tk.Theme.font.display * 2
                font.weight: Font.DemiBold
                font.letterSpacing: Tk.Theme.font.trackingWider
                Accessible.name: qsTr("Pairing code %1").arg(text.split("").join(" "))
            }
            Tk.Label {
                text: qsTr("Do these digits match the ones on your desktop?")
                muted: true
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
    }
    Tk.Grid {
        visible: root.phase === "confirm"
        columns: "1fr 1fr"
        columnGap: Tk.Theme.space.md
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button {
            objectName: "rejectButton"
            large: true
            variant: "danger"
            iconName: "x"
            text: qsTr("Reject")
            tooltip: qsTr("The codes differ: refuse this pairing")
            onClicked: link.reject()
        }
        Tk.Button {
            objectName: "acceptButton"
            large: true
            variant: "accent"
            iconName: "check"
            text: qsTr("Accept")
            tooltip: qsTr("The codes match: trust this desktop")
            onClicked: link.accept()
        }
    }

    // Waiting for the desktop to show its code
    Tk.Notice {
        visible: root.phase === "pairing"
        variant: "info"
        title: qsTr("Waiting for the desktop")
        text: qsTr("The code appears here as soon as the desktop answers.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    // Ready to pair
    Tk.EmptyState {
        visible: root.phase === "connected" && !link.paired
        iconName: "link"
        title: qsTr("Desktop found")
        text: qsTr("Start pairing, then compare the six digits on both screens.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button {
            objectName: "pairButton"
            large: true
            variant: "accent"
            text: qsTr("Pair with this desktop")
            onClicked: link.pair()
        }
    }

    // Stale pin
    Tk.Notice {
        visible: root.phase === "stale"
        variant: "warning"
        title: qsTr("Pairing needs renewing")
        text: link.message.length > 0 ? link.message
              : qsTr("This desktop no longer matches the saved pairing. Pair again if you trust it.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    Tk.Button {
        objectName: "pairAgainButton"
        visible: root.phase === "stale"
        large: true
        variant: "accent"
        text: qsTr("Pair again")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: link.pairAgain()
    }

    // Paired and talking
    Tk.Notice {
        visible: root.phase === "ready"
        variant: "success"
        title: qsTr("Paired")
        text: qsTr("%1 is trusted by this phone.").arg(link.pairedName.length > 0 ? link.pairedName : link.desktopName)
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    // Offline or failed
    Tk.Notice {
        visible: root.phase === "disconnected" || root.phase === "connecting" || root.phase === "error" || root.phase === "stopped"
        variant: root.phase === "error" ? "danger" : "info"
        title: LinkText.headline(root.phase, link.transport)
        text: qsTr("Plug the phone into the desktop with USB and start QindaLink there. %1").arg(link.message)
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    Tk.Button {
        objectName: "forgetButton"
        visible: link.paired
        variant: "outline"
        iconName: "unlink"
        text: qsTr("Forget this desktop")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: link.forget()
    }
}
