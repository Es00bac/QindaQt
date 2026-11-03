// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import QindaTK as Tk
import QindaQt.Link

// The open conversation: bubbles oldest to newest, older pages loaded when the top is reached,
// then the compose bar. Sending asks for a confirmation the first time in a session.
Tk.Flex {
    id: view
    objectName: "conversationView"
    direction: Tk.Flex.Column
    gap: 0

    Tk.Box {
        Tk.Flex.shrink: 0
        visible: Conversation.open
        implicitHeight: Tk.Theme.size.header
        paddingLeft: Tk.Theme.space.md
        paddingRight: Tk.Theme.space.md
        color: Tk.Theme.color.headerBg
        borderBottom: 1
        borderColor: Tk.Theme.color.divider
        Tk.Flex {
            anchors.fill: parent
            align: Tk.Flex.Center
            gap: Tk.Theme.space.sm
            Tk.Label {
                text: Conversation.title
                font.weight: Font.DemiBold
                Tk.Flex.shrink: 1
                Tk.Flex.minWidth: 0
            }
            Tk.Caption {
                text: Conversation.recipients !== Conversation.title ? Conversation.recipients : ""
                Tk.Flex.shrink: 1
                Tk.Flex.minWidth: 0
            }
            Tk.Spacer {}
            Tk.IconButton {
                iconName: "x"
                small: true
                tooltip: qsTr("Close conversation")
                onClicked: Conversation.close()
            }
        }
    }

    Item {
        Tk.Flex.grow: 1
        Tk.Flex.basis: 0
        Tk.Flex.minHeight: 0

        ListView {
            id: bubbles
            objectName: "messageView"
            anchors.fill: parent
            anchors.margins: Tk.Theme.space.sm
            clip: true
            visible: Conversation.open
            model: Conversation.messages
            spacing: Tk.Theme.space.xs
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: Tk.ScrollBar {}
            property bool stickToEnd: true

            // Older pages arrive above what is on screen; the view keeps its place.
            onAtYBeginningChanged: if (atYBeginning && Conversation.hasOlder) Conversation.loadOlder()
            onAtYEndChanged: stickToEnd = atYEnd
            onCountChanged: if (stickToEnd) Qt.callLater(positionViewAtEnd)

            delegate: MessageBubble {
                width: ListView.view.width
            }

            header: Tk.Caption {
                width: bubbles.width
                horizontalAlignment: Text.AlignHCenter
                visible: Conversation.loading
                height: visible ? implicitHeight + Tk.Theme.space.sm : 0
                text: qsTr("Loading...")
            }
        }

        Tk.EmptyState {
            anchors.centerIn: parent
            visible: !Conversation.open || (Conversation.error.length > 0 && bubbles.count === 0)
            iconName: Conversation.error.length > 0 ? "triangle-alert" : "message-square"
            title: Conversation.error.length > 0 ? qsTr("Could not load this conversation") : qsTr("Pick a conversation")
            text: Conversation.error.length > 0 ? Conversation.error : qsTr("Select one on the left to read and reply.")
        }
    }

    ComposeBar {
        Tk.Flex.shrink: 0
        visible: Conversation.open
        onSendRequested: text => Conversation.requestSend(text)
    }

    Tk.Dialog {
        id: confirm
        objectName: "sendConfirmDialog"
        property string recipientsText: ""
        property int characters: 0
        title: qsTr("Send this message?")
        subtitle: qsTr("It is sent as an SMS from your phone, which may cost money.")
        primaryText: qsTr("Send")
        secondaryText: qsTr("Cancel")
        onAccepted: Conversation.confirmSend()
        onRejected: Conversation.cancelSend()
        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.xs
            Tk.Overline { title: qsTr("To") }
            Tk.Label { text: confirm.recipientsText; wrapMode: Text.WordWrap; Tk.Flex.minWidth: 0 }
            Tk.Caption { text: qsTr("%1 characters. You will not be asked again this session.").arg(confirm.characters) }
        }
    }
    Connections {
        target: Conversation
        function onConfirmSendRequested(recipients, characters) {
            confirm.recipientsText = recipients
            confirm.characters = characters
            confirm.open()
        }
    }
}
