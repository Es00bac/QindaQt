// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// The controller features. Touchpad, Keyboard and Macros are live; Vault is still a placeholder. Each opens its own screen file.
ScreenFrame {
    id: root
    title: qsTr("Tools")
    subtitle: qsTr("Use the phone to control the desktop")

    signal openPage(string page)

    Repeater {
        model: [
            { page: "touchpad", title: qsTr("Touchpad"), detail: qsTr("Move the pointer and click"), icon: "mouse-pointer-2" },
            { page: "keyboard", title: qsTr("Keyboard"), detail: qsTr("Type into the focused window"), icon: "keyboard" },
            { page: "macros", title: qsTr("Macros"), detail: qsTr("Run your own key sequences"), icon: "zap" },
            { page: "vault", title: qsTr("Vault"), detail: qsTr("Passwords that never leave the phone"), icon: "lock" }
        ]
        delegate: Tk.Card {
            required property var modelData
            objectName: "toolRow_" + modelData.page
            interactive: true
            tooltip: modelData.title
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            onClicked: root.openPage(modelData.page)
            Tk.Flex {
                direction: Tk.Flex.Row
                align: Tk.Flex.Center
                gap: Tk.Theme.space.md
                Tk.Icon { name: modelData.icon; size: Tk.Theme.size.iconLg }
                Tk.Flex {
                    direction: Tk.Flex.Column
                    Tk.Flex.grow: 1
                    Tk.Flex.basis: 0
                    Tk.Label { text: modelData.title }
                    Tk.Caption { text: modelData.detail; muted: true; wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }
                }
                Tk.Badge { visible: modelData.page === "vault"; text: qsTr("Coming soon") }
            }
        }
    }
}
