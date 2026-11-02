// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Shared "does the desktop accept my input" banner for Touchpad, Keyboard and Macros.
// AGENT-CONTRACT: `link.inputState` is one of off, offline, pending, active, denied, unavailable, locked,
// busy, revoked (LinkBridge.h; the Java side is InputFeature.State). Nothing is shown while active.
// The first-time approval is the desktop's own remote-control prompt; this banner only explains it.
Tk.Notice {
    id: root
    objectName: "inputStateNotice"

    readonly property string inputState: link.inputState
    readonly property bool needsRetry: inputState === "denied" || inputState === "unavailable"
        || inputState === "revoked" || inputState === "busy"

    visible: inputState !== "active"
    Tk.Flex.alignSelf: Tk.Flex.Stretch
    variant: inputState === "pending" || inputState === "off" || inputState === "locked" ? "info"
        : inputState === "offline" ? "warning" : "danger"
    title: inputState === "pending" ? qsTr("Desktop must approve remote input once")
        : inputState === "denied" ? qsTr("The desktop did not approve")
        : inputState === "unavailable" ? qsTr("Remote input is not available")
        : inputState === "locked" ? qsTr("The desktop is locked")
        : inputState === "busy" ? qsTr("Another phone is controlling this desktop")
        : inputState === "revoked" ? qsTr("Remote input was turned off on the desktop")
        : inputState === "offline" ? qsTr("Not connected to a desktop")
        : qsTr("Starting")
    text: inputState === "pending" ? qsTr("Look at your desktop and approve the remote control request. You only do this once.")
        : inputState === "denied" ? qsTr("Ask again and approve the request on the desktop.")
        : inputState === "unavailable" ? qsTr("The desktop could not start remote input.")
        : inputState === "locked" ? qsTr("Unlock the desktop to use this phone as an input device.")
        : inputState === "busy" ? qsTr("Stop the other phone first.")
        : inputState === "revoked" ? qsTr("Ask again to approve it on the desktop.")
        : inputState === "offline" ? qsTr("Connect the phone to your desktop first.")
        : ""
    actions: [
        Tk.Button {
            objectName: "inputRetry"
            visible: root.needsRetry
            text: qsTr("Ask again")
            variant: "outline"
            small: true
            onClicked: link.inputBegin()
        }
    ]
}
