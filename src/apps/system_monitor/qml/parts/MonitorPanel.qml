// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// AGENT-CONTRACT: the frame every dashboard panel wears, in one of two
// modes. Framed, it is a Tk.Panel and its summary and controls sit in the
// header. Unframed (`framed: false`) it draws no header, because it is then
// inside a Tk.DockPanel that already supplies one, and the summary and
// controls move to a strip at the top of the body.
//
// AGENT-GUARD: there is exactly ONE instance of the controls, reparented
// between the two hosts by a binding. Declaring them per mode would give a
// panel two search fields with independent text, and the reader would end up
// typing into the one that is not filtering.
Tk.Panel {
    id: panel

    property string panelId: ""
    default property alias content: bodyHost.data
    property alias controls: controlHost.data
    // A short reading shown beside the title -- "98%", "31.2 GiB".
    property string summary: ""
    property color summaryColor: Tk.Theme.color.textMuted
    property bool detachable: true
    property bool framed: true

    signal detachRequested(string panelId)

    padding: panel.framed ? Tk.Theme.space.sm : 0
    collapsible: false
    headerVisible: panel.framed

    actions: Item {
        id: headerHost
        implicitWidth: childrenRect.width
        implicitHeight: Tk.Theme.size.controlSm
        visible: panel.framed
    }

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        gap: 0

        Tk.Box {
            id: strip
            visible: !panel.framed
            Tk.Flex.shrink: 0
            implicitHeight: strip.visible ? Tk.Theme.size.row : 0
            paddingLeft: Tk.Theme.space.sm
            paddingRight: Tk.Theme.space.xs
            color: "transparent"
        }

        Item {
            id: bodyHost
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Tk.Flex.minHeight: 0
            // Panels declare one root; it fills the body.
            onChildrenChanged: {
                for (let i = 0; i < bodyHost.children.length; ++i) {
                    bodyHost.children[i].anchors.fill = bodyHost
                }
            }
        }
    }

    // The single row of chrome, parented into whichever host is showing.
    // `framed` does not change during a panel's life, so this resolves once.
    Tk.Flex {
        id: chrome
        parent: panel.framed ? headerHost : strip.contentItem
        anchors.fill: panel.framed ? undefined : parent
        align: Tk.Flex.Center
        gap: Tk.Theme.space.sm

        Tk.Mono {
            visible: panel.summary.length > 0
            text: panel.summary
            color: panel.summaryColor
            Tk.Flex.shrink: 1
            Tk.Flex.minWidth: 0
        }
        Tk.Spacer { visible: !panel.framed }
        Tk.Flex {
            id: controlHost
            align: Tk.Flex.Center
            gap: Tk.Theme.space.xs
            visible: controlHost.children.length > 0
        }
        Tk.IconButton {
            visible: panel.detachable
            iconName: "external-link"
            small: true
            ghost: true
            tooltip: qsTr("Open this panel in its own window")
            onClicked: panel.detachRequested(panel.panelId)
        }
    }
}
