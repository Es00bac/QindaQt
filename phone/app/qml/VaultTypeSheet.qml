// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// "Type on desktop" (ADR-0440). The desktop reports which window has focus; the user confirms that
// target in plain text, and only then does the engine build the secret steps and send them. The
// confirmation lapses after 30 s (engine-side). No secret is ever read or held here: the state map
// holds only phase, app, title and a message.
// AGENT-GUARD: app and title come from the desktop and are shown as plain text (Tk.Label is PlainText).
// AGENT-CONTRACT: VaultBridge.typeState() phases: idle | requesting | confirm | need-unlock | sending | done | failed.
Tk.Card {
    id: root

    property string entryId: ""
    property var st: ({ phase: "idle", app: "", title: "", message: "" })
    property bool unlockAsked: false
    readonly property bool busy: st.phase !== "idle"
    signal closed()

    function begin(field) {
        unlockAsked = false
        vault.typeBegin(entryId, field)
        st = vault.typeState()
    }
    function cancel() {
        vault.typeCancel()
        st = vault.typeState()
        closed()
    }
    Component.onDestruction: vault.typeCancel()

    visible: busy
    Tk.Flex.alignSelf: Tk.Flex.Stretch

    // Mirrors the engine; also drives the unlock step (biometric) and the resumed confirm.
    Timer {
        interval: 300
        repeat: true
        running: root.busy
        onTriggered: {
            root.st = vault.typeState()
            vault.touch()
            if (root.st.phase === "need-unlock") {
                if (vault.state === "unlocked") vault.typeConfirm()
                else if (!root.unlockAsked) { root.unlockAsked = true; vault.unlock() }
            }
        }
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Tk.Heading { level: 3; text: qsTr("Type on desktop"); Tk.Flex.alignSelf: Tk.Flex.Stretch }

        Tk.Label {
            objectName: "typeStatus"
            visible: root.st.phase === "requesting" || root.st.phase === "sending" || root.st.phase === "need-unlock"
            wrapMode: Text.Wrap
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            text: root.st.phase === "requesting" ? qsTr("Asking the desktop which window has focus…")
                : root.st.phase === "need-unlock" ? qsTr("Unlock the vault to continue.")
                : qsTr("Typing on the desktop…")
        }

        Tk.Flex {
            visible: root.st.phase === "confirm"
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.Caption { text: qsTr("The desktop will type into this window"); muted: true }
            Tk.Label { objectName: "typeApp"; text: root.st.app || qsTr("(unknown application)"); wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }
            Tk.Label { objectName: "typeTitle"; text: root.st.title || qsTr("(no title)"); wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }
            Tk.Caption {
                text: qsTr("Check that this is the right window. If focus moves, typing stops. This confirmation expires in 30 seconds.")
                muted: true
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Button { objectName: "typeConfirm"; text: qsTr("Type here"); iconName: "monitor"; Tk.Flex.alignSelf: Tk.Flex.Stretch
                onClicked: vault.typeConfirm() }
        }

        Tk.Notice {
            visible: root.st.phase === "failed"
            variant: "danger"
            text: root.st.message
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Notice {
            visible: root.st.phase === "done"
            text: root.st.message
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }

        Tk.Button {
            objectName: "typeClose"
            text: root.st.phase === "done" || root.st.phase === "failed" ? qsTr("Close") : qsTr("Cancel")
            variant: "outline"
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            onClicked: root.cancel()
        }
    }
}
