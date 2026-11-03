// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Password and passphrase generator. The result is offered through `use(value)`; the panel keeps
// `value` only for display and clears it when destroyed. Length 8-128, character classes, or 3-12
// words from the bundled EFF large list (CC BY 3.0 US, attributed in the panel and ADR-0439).
Tk.Card {
    id: root

    property string value: ""
    property bool words: false
    property int length: 20
    property int wordCount: 5
    signal use(string value)

    function regenerate() {
        value = words
            ? vault.generatePassphrase(wordCount, "-", true, true)
            : vault.generatePassword(length, lowerBox.checked, upperBox.checked, digitBox.checked, symbolBox.checked, ambigBox.checked)
    }
    Component.onCompleted: regenerate()
    Component.onDestruction: value = ""

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        Tk.Segmented {
            objectName: "genKind"
            model: [qsTr("Password"), qsTr("Passphrase")]
            currentIndex: root.words ? 1 : 0
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            onActivated: index => { root.words = index === 1; root.regenerate() }
        }
        Tk.Label {
            objectName: "genValue"
            text: root.value
            mono: true
            wrapMode: Text.WrapAnywhere
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Flex {
            visible: !root.words
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.xs
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.Label { text: qsTr("Length %1").arg(root.length) }
            Tk.Slider {
                objectName: "genLength"
                from: 8; to: 128; stepSize: 1
                value: root.length
                tooltip: qsTr("Password length")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                onValueModified: v => { root.length = Math.round(v); root.regenerate() }
            }
            Tk.CheckBox { id: lowerBox; text: qsTr("Lower case a-z"); checked: true; onToggled: root.regenerate() }
            Tk.CheckBox { id: upperBox; text: qsTr("Upper case A-Z"); checked: true; onToggled: root.regenerate() }
            Tk.CheckBox { id: digitBox; text: qsTr("Digits 0-9"); checked: true; onToggled: root.regenerate() }
            Tk.CheckBox { id: symbolBox; text: qsTr("Symbols"); checked: true; onToggled: root.regenerate() }
            Tk.CheckBox { id: ambigBox; text: qsTr("Avoid look-alikes (0 O 1 l I)"); checked: false; onToggled: root.regenerate() }
        }
        Tk.Flex {
            visible: root.words
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.xs
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.Label { text: qsTr("%1 words").arg(root.wordCount) }
            Tk.Slider {
                objectName: "genWords"
                from: 3; to: 12; stepSize: 1
                value: root.wordCount
                tooltip: qsTr("Number of words")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                onValueModified: v => { root.wordCount = Math.round(v); root.regenerate() }
            }
        }
        Tk.Caption {
            objectName: "genAttribution"
            visible: root.words
            text: qsTr("Word list: EFF large wordlist, (c) Electronic Frontier Foundation, licensed CC BY 3.0 US (creativecommons.org/licenses/by/3.0/us).")
            muted: true
            wrapMode: Text.Wrap
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Flex {
            direction: Tk.Flex.Row
            gap: Tk.Theme.space.sm
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.Button { objectName: "genAgain"; text: qsTr("Another"); iconName: "refresh-cw"; variant: "outline"; Tk.Flex.grow: 1
                onClicked: root.regenerate() }
            Tk.Button { objectName: "genUse"; text: qsTr("Use this"); iconName: "check"; variant: "accent"; Tk.Flex.grow: 1
                onClicked: root.use(root.value) }
        }
    }
}
