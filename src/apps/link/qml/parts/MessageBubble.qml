// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import QindaQt.Link

// One message: a bubble on the sender's side, an MMS image scaled to fit (fetched when the bubble
// is first shown), the time, and a send status under messages this app sent.
Item {
    id: bubble

    required property string key
    required property string body
    required property string when
    required property bool outgoing
    required property string imageState
    required property string image
    required property string status
    required property string statusText
    required property bool trunc

    implicitHeight: column.implicitHeight

    Component.onCompleted: if (imageState === "none") Conversation.loadImage(key)

    Tk.Flex {
        id: column
        width: parent.width
        direction: Tk.Flex.Column
        align: bubble.outgoing ? Tk.Flex.End : Tk.Flex.Start
        gap: 2

        Tk.Box {
            id: card
            objectName: "bubble"
            Tk.Flex.shrink: 1
            Tk.Flex.maxWidth: Math.max(160, bubble.width * 0.72)
            padding: Tk.Theme.space.sm
            radius: Tk.Theme.radius.md
            color: bubble.outgoing ? Tk.Theme.color.accentSubtle : Tk.Theme.color.controlBg
            borderWidth: 1
            borderColor: bubble.outgoing ? Tk.Theme.color.accent : Tk.Theme.color.border

            Tk.Flex {
                direction: Tk.Flex.Column
                gap: Tk.Theme.space.xs
                Image {
                    visible: bubble.imageState === "ready"
                    source: bubble.image
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: 480
                    Tk.Flex.shrink: 0
                    width: Math.min(sourceSize.width, 240)
                    height: status === Image.Ready ? width * implicitHeight / Math.max(1, implicitWidth) : 0
                }
                Tk.Caption {
                    visible: bubble.imageState === "loading" || bubble.imageState === "none"
                    text: qsTr("Loading image...")
                }
                Tk.Caption {
                    visible: bubble.imageState === "failed"
                    text: qsTr("Image unavailable")
                }
                Tk.Label {
                    visible: bubble.body.length > 0
                    text: bubble.body
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                    selectable: true
                    Tk.Flex.shrink: 1
                    Tk.Flex.minWidth: 0
                }
            }
        }
        Tk.Caption {
            text: bubble.when + (bubble.trunc ? " " + qsTr("(shortened)") : "")
                  + (bubble.statusText.length > 0 ? "  " + bubble.statusText : "")
            color: bubble.status === "failed" ? Tk.Theme.color.danger : Tk.Theme.color.textMuted
        }
    }
}
