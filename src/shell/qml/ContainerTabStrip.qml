// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root

    required property var theme
    readonly property var colors: theme.colors ?? ({})
    readonly property var decoration: theme.decoration ?? ({})
    readonly property bool rightToLeft: decoration.tabDirection === "right-to-left"

    Row {
        anchors.left: root.rightToLeft ? undefined : parent.left
        anchors.right: root.rightToLeft ? parent.right : undefined
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5
        layoutDirection: root.rightToLeft ? Qt.RightToLeft : Qt.LeftToRight

        Repeater {
            model: [qsTr("Workspace"), qsTr("Research"), qsTr("Single window")]

            Rectangle {
                required property string modelData

                width: tabLabel.implicitWidth + 22
                height: 28
                radius: 7
                color: index === 0 ? root.colors.accent ?? "#8fc8b7" : "transparent"

                Text {
                    id: tabLabel

                    anchors.centerIn: parent
                    text: modelData
                    color: index === 0 ? root.colors.accentText ?? "#10201b"
                                       : root.colors.textMuted ?? "#a9afa9"
                    font.pixelSize: 11
                }
            }
        }
    }
}
