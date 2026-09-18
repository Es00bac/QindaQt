// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// Where the pen lands: the screen as a rectangle, the mapped part of it
// filled, and the rest dimmed. It is a picture of `area`, never a control —
// the route writes the area and this only shows what was written.
Item {
    id: root

    // Normalized rectangle of the screen the tablet reaches.
    property real areaX: 0
    property real areaY: 0
    property real areaWidth: 1
    property real areaHeight: 1
    // Screen proportions, so the outline is the shape of the real screen.
    property real screenAspect: 16 / 9
    property string caption: ""

    readonly property real frameWidth: Math.min(width, height * root.screenAspect)
    readonly property real frameHeight: root.frameWidth / root.screenAspect

    implicitWidth: 240
    implicitHeight: 160
    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Tablet mapping diagram")
    Accessible.description: root.caption.length > 0
        ? root.caption
        : qsTr("The pen reaches %1 percent of the screen width and %2 percent of its height.")
            .arg(Math.round(root.areaWidth * 100)).arg(Math.round(root.areaHeight * 100))

    Rectangle {
        id: screenFrame
        objectName: "tabletDiagramScreen"
        anchors.centerIn: parent
        width: root.frameWidth
        height: root.frameHeight
        radius: Tokens.radius.s
        color: Tokens.bg.sunken
        border.color: Tokens.outline.divider
        border.width: 1

        Rectangle {
            objectName: "tabletDiagramArea"
            x: screenFrame.width * Math.max(0, Math.min(1, root.areaX))
            y: screenFrame.height * Math.max(0, Math.min(1, root.areaY))
            width: screenFrame.width * Math.max(0, Math.min(1, root.areaWidth))
            height: screenFrame.height * Math.max(0, Math.min(1, root.areaHeight))
            radius: Tokens.radius.s
            color: Tokens.accent.subtle
            border.color: Tokens.accent.default
            border.width: 2
        }
    }

    Text {
        objectName: "tabletDiagramCaption"
        visible: root.caption.length > 0
        anchors.top: screenFrame.bottom
        anchors.topMargin: Tokens.space["2"]
        anchors.horizontalCenter: screenFrame.horizontalCenter
        width: root.width
        horizontalAlignment: Text.AlignHCenter
        text: root.caption
        color: Tokens.fg.muted
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.caption
        wrapMode: Text.Wrap
        Accessible.ignored: true
    }
}
