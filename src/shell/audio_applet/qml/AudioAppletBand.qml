// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0

// One collapsible band header: a caption in the desk idiom, a disclosure
// chevron, and the hairline that separates this band from the one above it.
// Every section of the panel uses this, so the hierarchy is one weight
// everywhere rather than a full SectionHeader for one band and a faded
// caption for the next.
Item {
    id: band

    required property string title
    required property bool expanded
    signal toggled()

    implicitHeight: 22

    Accessible.role: Accessible.Button
    Accessible.name: band.title
    Accessible.description: band.expanded
        ? qsTr("Collapse the %1 section").arg(band.title)
        : qsTr("Expand the %1 section").arg(band.title)
    Accessible.onPressAction: band.toggled()

    activeFocusOnTab: true
    Keys.onReturnPressed: event => { band.toggled(); event.accepted = true }
    Keys.onEnterPressed: event => { band.toggled(); event.accepted = true }
    Keys.onSpacePressed: event => { band.toggled(); event.accepted = true }

    RowLayout {
        anchors.fill: parent
        spacing: Tokens.space["2"]

        Text {
            objectName: "audioBandChevron"
            text: band.expanded ? "▾" : "▸"
            color: Tokens.fg.muted
            font: Qt.font({ family: Tokens.type.fontFamily,
                            pointSize: Tokens.type.caption })
            Accessible.ignored: true
        }

        Text {
            objectName: "audioBandTitle"
            text: band.title.toUpperCase()
            color: Tokens.fg.muted
            font: Qt.font({ family: Tokens.type.fontFamily,
                            pointSize: Tokens.type.caption,
                            weight: Font.DemiBold,
                            letterSpacing: 1.0 })
            Accessible.ignored: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Tokens.outline.divider
            Accessible.ignored: true
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.space["1"]
        radius: Tokens.radius.s
        color: band.activeFocus ? Tokens.state.hover : "transparent"
        border.width: band.activeFocus ? 1 : 0
        border.color: Tokens.accent.default
        z: -1
        Accessible.ignored: true
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            band.forceActiveFocus(Qt.MouseFocusReason)
            band.toggled()
        }
    }
}
