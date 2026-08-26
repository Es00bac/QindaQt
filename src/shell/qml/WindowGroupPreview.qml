// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QindaQt.Shell

Rectangle {
    id: root

    required property var theme
    required property var profile
    readonly property var colors: theme.colors ?? ({})

    radius: theme.cornerRadius ?? 10
    color: colors.surface ?? "#222624"
    border.color: colors.border ?? "#3c433f"
    border.width: 1
    clip: true
    z: 2

    Column {
        anchors.fill: parent

        Rectangle {
            id: containerTitleBar

            width: parent.width
            height: 38
            color: root.colors.surfaceRaised ?? "#2c312e"
            border.color: root.colors.border ?? "#3c433f"

            DecorationButtons {
                id: windowButtons

                anchors.left: root.theme.decoration?.buttonPlacement === "left" ? parent.left : undefined
                anchors.right: root.theme.decoration?.buttonPlacement === "left" ? undefined : parent.right
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                theme: root.theme
            }

            ContainerTabStrip {
                anchors.left: root.theme.decoration?.buttonPlacement === "left"
                              ? windowButtons.right : parent.left
                anchors.right: root.theme.decoration?.buttonPlacement === "left"
                               ? parent.right : windowButtons.left
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                theme: root.theme
            }
        }

        SplitView {
            width: parent.width
            height: parent.height - 38
            orientation: Qt.Horizontal

            MemberWindow {
                SplitView.preferredWidth: parent.width * 0.42
                title: qsTr("Files — QindaQt")
                detail: qsTr("src   docs   data   tests   tools")
                glyph: "▦"
                theme: root.theme
            }

            MemberWindow {
                SplitView.fillWidth: true
                title: qsTr("Terminal — agent workspace")
                detail: qsTr("$ explain and build the active module\n\nCommand preview required before execution")
                glyph: ">_"
                theme: root.theme
                terminal: true
            }
        }
    }
}
