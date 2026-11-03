// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.Link
import "../parts" as Parts

// Messages: the conversation list beside the open conversation. The RCS strip sits above both.
Tk.Flex {
    id: page
    objectName: "messagesPage"
    direction: Tk.Flex.Column
    gap: 0

    Parts.ServiceNotice { Tk.Flex.shrink: 0 }

    Tk.Notice {
        objectName: "rcsNotice"
        Tk.Flex.shrink: 0
        visible: !Prefs.rcsNoticeDismissed && Devices.phoneReady
        variant: "info"
        dismissible: true
        text: qsTr("Only SMS and MMS appear here. RCS chats (Google Messages chat features) live outside "
                   + "the phone's message store, so QindaLink cannot see or answer them.")
        onDismissed: Prefs.rcsNoticeDismissed = true
    }

    Item {
        visible: !Devices.phoneReady
        Tk.Flex.grow: 1
        Tk.Flex.basis: 0
        Tk.EmptyState {
            anchors.centerIn: parent
            width: Math.min(parent.width - 32, 360)
            maxTextWidth: width
            iconName: "message-square"
            title: qsTr("No phone connected")
            text: qsTr("Messages appear here once a paired phone is connected over USB.")
        }
    }

    Tk.Flex {
        visible: Devices.phoneReady
        Tk.Flex.grow: 1
        Tk.Flex.basis: 0
        Tk.Flex.minHeight: 0
        direction: Tk.Flex.Row

        Parts.ThreadList {
            Tk.Flex.shrink: 0
            implicitWidth: 320
        }
        Tk.Divider { vertical: true; Tk.Flex.shrink: 0 }
        Parts.ConversationView {
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Tk.Flex.minWidth: 0
        }
    }
}
