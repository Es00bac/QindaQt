// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// The phone as the desktop's keyboard: a text field sent on Send (or live), a key row and one-shot Ctrl/Alt/Shift/Meta latches.
// AGENT-GUARD: the text field uses sensitive input hints so the phone keyboard neither learns nor suggests what is typed to
// the desktop. Typed text is never logged, stored or kept after Send.
// AGENT-CONTRACT: Main.qml starts remote input while this page is shown.
ScreenFrame {
    id: root
    title: qsTr("Keyboard")
    subtitle: qsTr("Type into the focused desktop window")
    backVisible: true

    readonly property bool live: link.inputState === "active"
    // Bit masks match KeyChord.java: Ctrl 1, Alt 2, Shift 4, Meta 8.
    property int mods: 0
    property string previous: ""

    function toggle(bit) { mods = (mods & bit) ? (mods & ~bit) : (mods | bit) }

    function sendKey(name) {
        link.sendKey(name, mods)
        mods = 0 // latches are one-shot
    }

    // Live mode: send only what changed since the last edit; deletions become BackSpace.
    function liveEdit(text) {
        let common = 0
        const max = Math.min(previous.length, text.length)
        while (common < max && previous.charAt(common) === text.charAt(common)) ++common
        for (let i = common; i < previous.length; ++i) link.sendKey("BackSpace", 0)
        const added = text.substring(common)
        if (added.length > 0) {
            if (mods !== 0 && added.length === 1) {
                link.sendChar(added.codePointAt(0), mods)
                mods = 0
            } else {
                link.sendText(added)
            }
        }
        previous = text
    }

    InputStateNotice { }

    Tk.TextField {
        id: field
        objectName: "textInput"
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        placeholderText: qsTr("Type here")
        enabled: root.live
        tooltip: qsTr("Text to send to the desktop")
        // Sensitive: no learning, suggestions or autocorrect for what is typed to the desktop.
        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhMultiLine
        onTextEdited: if (liveSwitch.checked) root.liveEdit(text)
        onAccepted: if (!liveSwitch.checked) sendButton.clicked()
    }

    Tk.Flex {
        direction: Tk.Flex.Row
        align: Tk.Flex.Center
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Switch {
            id: liveSwitch
            objectName: "liveSwitch"
            text: qsTr("Type as I go")
            tooltip: qsTr("Send each character immediately")
            onToggled: { root.previous = ""; field.text = "" }
        }
        Item { Tk.Flex.grow: 1; Tk.Flex.basis: 0 }
        Tk.Button {
            id: sendButton
            objectName: "sendText"
            text: qsTr("Send")
            variant: "accent"
            enabled: root.live && !liveSwitch.checked && field.text.length > 0
            onClicked: {
                link.sendText(field.text)
                field.text = ""
            }
        }
    }

    Tk.Label { text: qsTr("Keys") }

    Tk.Flex {
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Repeater {
            model: [ { text: "Ctrl", bit: 1 }, { text: "Alt", bit: 2 }, { text: "Shift", bit: 4 }, { text: "Meta", bit: 8 } ]
            delegate: Tk.Button {
                required property var modelData
                objectName: "latch" + modelData.text
                text: modelData.text
                // Latched = filled accent; the latch clears after the next key.
                variant: (root.mods & modelData.bit) !== 0 ? "accent" : "outline"
                enabled: root.live
                tooltip: qsTr("Hold %1 for the next key").arg(modelData.text)
                Tk.Flex.grow: 1
                Tk.Flex.basis: 0
                onClicked: root.toggle(modelData.bit)
            }
        }
    }

    Tk.Flex {
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Repeater {
            model: [ { text: "Esc", name: "Escape" }, { text: "Tab", name: "Tab" }, { text: "Enter", name: "Return" },
                     { text: "Bksp", name: "BackSpace" } ]
            delegate: Tk.Button {
                required property var modelData
                objectName: "key" + modelData.name
                text: modelData.text
                enabled: root.live
                Tk.Flex.grow: 1
                Tk.Flex.basis: 0
                onClicked: root.sendKey(modelData.name)
            }
        }
    }

    Tk.Flex {
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Repeater {
            model: [ { icon: "arrow-left", name: "Left" }, { icon: "arrow-up", name: "Up" },
                     { icon: "arrow-down", name: "Down" }, { icon: "arrow-right", name: "Right" } ]
            delegate: Tk.IconButton {
                required property var modelData
                objectName: "key" + modelData.name
                iconName: modelData.icon
                tooltip: modelData.name
                enabled: root.live
                Tk.Flex.grow: 1
                Tk.Flex.basis: 0
                onClicked: root.sendKey(modelData.name)
            }
        }
    }
}
