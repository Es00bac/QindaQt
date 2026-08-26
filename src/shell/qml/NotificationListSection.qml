// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root

    required property string heading
    required property var notificationModel
    required property var presentation
    required property var theme
    readonly property var colors: theme.colors ?? ({})
    readonly property alias count: notificationList.count

    implicitHeight: headingLabel.implicitHeight + 8 + notificationList.contentHeight

    Text {
        id: headingLabel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        text: root.heading
        color: root.colors.textMuted ?? "#a9afa9"
        font.pixelSize: 12
        font.bold: true
        textFormat: Text.PlainText
    }

    ListView {
        id: notificationList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headingLabel.bottom
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        spacing: 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.notificationModel

        delegate: NotificationCard {
            width: notificationList.width
            presentation: root.presentation
            theme: root.theme
        }
    }
}
