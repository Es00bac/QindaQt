// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The touch conventions (ADR-0193) under test: a TouchContextArea that must
// fire only for a held finger, and a Button whose height follows the touch
// token.
Item {
    id: root
    objectName: "touchProbeRoot"
    width: 400
    height: 300

    property int contextRequests: 0
    property point lastContextPosition: Qt.point(-1, -1)
    property int rightClicks: 0

    Rectangle {
        objectName: "touchTarget"
        x: 20
        y: 20
        width: 200
        height: 120
        color: Tokens.bg.raised

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: root.rightClicks += 1
        }

        TouchContextArea {
            objectName: "touchContext"
            anchors.fill: parent
            onContextRequested: (position) => {
                root.contextRequests += 1
                root.lastContextPosition = position
            }
        }
    }

    Button {
        objectName: "touchButton"
        x: 20
        y: 180
        text: "Open"
    }
}
