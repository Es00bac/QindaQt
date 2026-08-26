// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Window {
    id: root

    required property var presentation
    required property var theme
    readonly property var colors: theme.colors ?? ({})

    visible: false
    color: "transparent"
    title: qsTr("QindaQt notifications")
    flags: Qt.FramelessWindowHint

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Item {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 38

            Button {
                id: historyButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 90
                height: 32
                text: qsTr("History")
                focusPolicy: Qt.TabFocus
                Accessible.name: qsTr("Open notification center")
                onClicked: root.presentation.setCenterOpen(true)
            }

            NotificationOperationStatus {
                anchors.left: parent.left
                anchors.right: historyButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                presentation: root.presentation
                theme: root.theme
            }
        }

        ListView {
            id: popupList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
            spacing: 10
            clip: true
            model: root.presentation.popupModel
            boundsBehavior: Flickable.StopAtBounds

            delegate: NotificationCard {
                width: popupList.width
                presentation: root.presentation
                theme: root.theme
                popup: true
            }
        }
    }
}
