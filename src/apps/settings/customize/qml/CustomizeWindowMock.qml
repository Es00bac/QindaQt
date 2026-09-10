// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// Decorative application window behind the panels. Pure presentation from
// live tokens: it gives the WYSIWYG monitor depth and shows how translucent
// panel materials sit over real content. It is never interactive.
Rectangle {
    id: root

    readonly property real titleBarHeight: Math.max(
        Tokens.space["5"], height * 0.16)

    radius: Tokens.radius.m
    color: Tokens.bg.raised
    border.width: Tokens.space["1"] / 2
    border.color: Tokens.outline.strong
    opacity: 0.96
    Accessible.ignored: true

    Rectangle {
        id: titleBar

        width: parent.width
        height: root.titleBarHeight
        radius: parent.radius
        color: Tokens.bg.highest
        border.color: Tokens.outline.divider
        border.width: parent.border.width

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: parent.radius
            color: parent.color
        }

        Row {
            spacing: Tokens.space["2"]
            anchors.left: parent.left
            anchors.leftMargin: Tokens.space["3"]
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: [Tokens.danger.default, Tokens.status.warning.background,
                        Tokens.status.success.background]

                delegate: Rectangle {
                    required property var modelData
                    width: Math.max(Tokens.space["3"], root.titleBarHeight * 0.22)
                    height: width
                    radius: width / 2
                    color: modelData
                }
            }
        }
    }

    // Content suggestion: two quiet text lines instead of real text.
    Column {
        anchors.fill: parent
        anchors.topMargin: root.titleBarHeight + Tokens.space["4"]
        anchors.leftMargin: Tokens.space["4"]
        anchors.rightMargin: Tokens.space["4"]
        spacing: Tokens.space["3"]

        Repeater {
            model: [0.86, 0.62]

            delegate: Rectangle {
                required property var modelData
                width: parent.width * modelData
                height: Tokens.space["2"]
                radius: height / 2
                color: Tokens.fg.muted
                opacity: 0.3
            }
        }
    }
}
