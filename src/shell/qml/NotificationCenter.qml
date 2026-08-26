// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Window {
    id: root

    required property var presentation
    required property var theme
    readonly property var colors: theme.colors ?? ({})

    visible: false
    color: "transparent"
    title: qsTr("Notification center")
    flags: Qt.FramelessWindowHint

    Rectangle {
        anchors.fill: parent
        radius: Math.min(root.theme.cornerRadius ?? 10, 16)
        color: root.colors.surface ?? "#222624"
        border.color: root.colors.border ?? "#3c433f"
        border.width: 1

        Text {
            id: titleText
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16
            text: qsTr("Notifications")
            color: root.colors.text ?? "#f2f1eb"
            font.pixelSize: 20
            font.bold: true
            textFormat: Text.PlainText
        }

        Button {
            id: clearButton
            anchors.right: closeButton.left
            anchors.rightMargin: 6
            anchors.verticalCenter: titleText.verticalCenter
            text: qsTr("Clear history")
            enabled: historySection.count > 0
            focusPolicy: Qt.TabFocus
            onClicked: root.presentation.clearHistory()
        }

        NotificationOperationStatus {
            anchors.left: titleText.right
            anchors.leftMargin: 12
            anchors.right: clearButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: titleText.verticalCenter
            presentation: root.presentation
            theme: root.theme
        }

        ToolButton {
            id: closeButton
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            width: 36
            height: 36
            text: "×"
            focusPolicy: Qt.TabFocus
            Accessible.name: qsTr("Close notification center")
            onClicked: root.presentation.setCenterOpen(false)
        }

        NotificationListSection {
            id: activeSection
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleText.bottom
            anchors.margins: 12
            height: Math.min(implicitHeight, 300)
            heading: qsTr("Active")
            notificationModel: root.presentation.activeModel
            presentation: root.presentation
            theme: root.theme
        }

        NotificationListSection {
            id: historySection
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: activeSection.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 12
            heading: qsTr("Recent")
            notificationModel: root.presentation.historyModel
            presentation: root.presentation
            theme: root.theme
        }

        Text {
            anchors.centerIn: parent
            visible: activeSection.count === 0 && historySection.count === 0
            text: qsTr("No notifications")
            color: root.colors.textMuted ?? "#a9afa9"
            font.pixelSize: 14
            textFormat: Text.PlainText
        }
    }
}
