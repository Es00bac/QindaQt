// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Shared page chrome: a title row (optional back button) above a vertically scrolling body.
// AGENT-NOTE: every screen is one self-contained file that wraps its content in a ScreenFrame, so a
// later lane replaces a screen by replacing its file. Content goes in as children and is stacked
// in a column by the inner Tk.Flex.
Tk.Flex {
    id: frame

    property string title: ""
    property string subtitle: ""
    property bool backVisible: false
    default property alias body: bodyColumn.data
    signal backRequested()

    direction: Tk.Flex.Column
    gap: Tk.Theme.space.sm

    Tk.Flex {
        direction: Tk.Flex.Row
        align: Tk.Flex.Center
        gap: Tk.Theme.space.sm
        paddingLeft: Tk.Theme.space.md
        paddingRight: Tk.Theme.space.md
        paddingTop: Tk.Theme.space.md
        Tk.Flex.shrink: 0

        Tk.IconButton {
            visible: frame.backVisible
            iconName: "chevron-left"
            tooltip: qsTr("Back")
            objectName: "backButton"
            onClicked: frame.backRequested()
        }
        Tk.Flex {
            direction: Tk.Flex.Column
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Tk.Heading { text: frame.title; level: 2; objectName: "screenTitle" }
            Tk.Caption {
                visible: frame.subtitle.length > 0
                text: frame.subtitle
                muted: true
            }
        }
    }

    Tk.Scroll {
        Tk.Flex.grow: 1
        Tk.Flex.basis: 0
        accessibleName: frame.title
        Tk.Flex {
            id: bodyColumn
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.md
            padding: Tk.Theme.space.md
        }
    }
}
