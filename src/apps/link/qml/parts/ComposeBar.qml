// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.Link

// Compose box and the Send button. Enter makes a new line; only the Send button sends, and
// nothing here ever submits a draft on its own.
Tk.Box {
    id: bar

    signal sendRequested(string text)

    padding: Tk.Theme.space.sm
    color: Tk.Theme.color.panel
    borderTop: 1
    borderColor: Tk.Theme.color.divider

    Connections {
        target: Conversation
        function onSendAccepted() { draft.text = "" }
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.xs

        Tk.Notice {
            visible: Conversation.error.length > 0
            variant: "danger"
            text: Conversation.error
        }
        Tk.Flex {
            gap: Tk.Theme.space.sm
            align: Tk.Flex.End
            Tk.TextArea {
                id: draft
                objectName: "composeField"
                rows: 2
                placeholderText: qsTr("Message")
                tooltip: qsTr("Write a message; press Send to send it")
                Tk.Flex.grow: 1
                Tk.Flex.basis: 0
                Tk.Flex.minWidth: 0
            }
            Tk.Button {
                objectName: "sendButton"
                text: qsTr("Send")
                iconName: "send"
                variant: "accent"
                busy: Conversation.sending
                enabled: draft.text.trim().length > 0 && !Conversation.sending && draft.text.length <= 1600
                tooltip: qsTr("Send as an SMS from your phone")
                onClicked: bar.sendRequested(draft.text)
            }
        }
        Tk.Caption {
            horizontalAlignment: Text.AlignRight
            visible: draft.text.length > 1200
            text: qsTr("%1 / 1600").arg(draft.text.length)
        }
    }
}
