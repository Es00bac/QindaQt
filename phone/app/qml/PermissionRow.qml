// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// One line of the Permissions checklist. `state` is "granted", "denied" or "later" (not built yet).
Tk.Card {
    id: row

    property string permKey: ""
    property string title: ""
    property string detail: ""
    property string iconName: "settings"
    property string state: "later"
    property string actionText: qsTr("Open settings")
    signal openRequested(string key)

    objectName: "perm_" + permKey

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        Tk.Flex {
            direction: Tk.Flex.Row
            align: Tk.Flex.Center
            gap: Tk.Theme.space.sm
            Tk.Icon { name: row.iconName; size: Tk.Theme.size.iconLg }
            Tk.Flex {
                direction: Tk.Flex.Column
                Tk.Flex.grow: 1
                Tk.Flex.basis: 0
                Tk.Label { text: row.title; wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }
                Tk.Caption { text: row.detail; muted: true; wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }
            }
            Tk.Badge {
                objectName: "permState"
                text: row.state === "granted" ? qsTr("Allowed")
                    : row.state === "denied" ? qsTr("Not allowed") : qsTr("Coming soon")
                variant: row.state === "granted" ? "success" : row.state === "denied" ? "warning" : "default"
            }
        }
        Tk.Button {
            objectName: "permAction"
            visible: row.state === "denied"
            text: row.actionText
            variant: "outline"
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            onClicked: row.openRequested(row.permKey)
        }
    }
}
