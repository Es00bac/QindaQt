// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The pad: the buttons, rings, strips and dials KWin reports on this tablet.
//
// AGENT-CONTRACT: KWin exposes the COUNTS of pad controls but no binding
// interface — pad buttons reach applications as ordinary key events that
// global shortcuts capture. This section therefore states what the hardware
// has and sends the user to the Shortcuts destination to bind it, rather
// than offering a picker that could not write anything.
ColumnLayout {
    id: root

    required property var selection

    readonly property int buttons: root.selection !== null
                                   ? root.selection.padButtonCount : -1
    readonly property int rings: root.selection !== null
                                 ? root.selection.padRingCount : -1
    readonly property int strips: root.selection !== null
                                  ? root.selection.padStripCount : -1
    readonly property int dials: root.selection !== null
                                 ? root.selection.padDialCount : -1

    function describe() {
        const parts = []
        if (root.buttons > 0)
            parts.push(qsTr("%n button(s)", "", root.buttons))
        if (root.rings > 0)
            parts.push(qsTr("%n ring(s)", "", root.rings))
        if (root.strips > 0)
            parts.push(qsTr("%n strip(s)", "", root.strips))
        if (root.dials > 0)
            parts.push(qsTr("%n dial(s)", "", root.dials))
        if (parts.length === 0)
            return qsTr("This tablet's pad reports no controls.")
        return qsTr("This tablet's pad has %1.").arg(parts.join(qsTr(", ")))
    }

    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Pad buttons")
        description: qsTr("The buttons, rings and strips on the tablet itself")
    }

    Label {
        objectName: "tabletPadSummary"
        Layout.fillWidth: true
        text: root.describe()
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Label {
        objectName: "tabletPadBindingNote"
        Layout.fillWidth: true
        muted: true
        text: qsTr("Pad controls arrive as ordinary keys. Bind them under Input → Shortcuts.")
    }
}
