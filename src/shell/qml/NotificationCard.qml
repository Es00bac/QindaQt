// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    required property var presentation
    required property var theme
    required property double notificationId
    required property string applicationName
    required property string summary
    required property string body
    required property int urgency
    required property var actions
    required property bool active
    property bool popup: false
    readonly property var colors: theme.colors ?? ({})
    readonly property int actionCount: typeof actions.count === "number"
                                       ? actions.count : actions.length
    readonly property int primaryActionCount: width >= 390
                                              ? Math.min(actionCount, 2)
                                              : width >= 300
                                                ? Math.min(actionCount, 1) : 0

    function actionAt(index) {
        return typeof actions.get === "function" ? actions.get(index)
                                                  : actions[index];
    }

    implicitHeight: 136
    radius: Math.min(theme.cornerRadius ?? 10, 14)
    color: colors.surfaceRaised ?? "#2c312e"
    border.width: urgency === 2 ? 2 : 1
    border.color: urgency === 2 ? colors.danger ?? "#f07c76"
                                  : colors.border ?? "#3c433f"
    Accessible.role: Accessible.AlertMessage
    Accessible.name: summary.length > 0 ? summary : applicationName
    Accessible.description: body

    Rectangle {
        id: identity
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 12
        width: 32
        height: 32
        radius: 10
        color: root.urgency === 2 ? root.colors.danger ?? "#f07c76"
                                  : root.colors.accent ?? "#8fc8b7"

        Text {
            anchors.centerIn: parent
            text: root.applicationName.length > 0
                  ? root.applicationName.slice(0, 1).toUpperCase() : "Q"
            color: root.colors.accentText ?? "#10201b"
            font.bold: true
        }
    }

    Text {
        id: appName
        anchors.left: identity.right
        anchors.leftMargin: 10
        anchors.right: closeButton.left
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 10
        text: root.applicationName.length > 0 ? root.applicationName : qsTr("Notification")
        color: root.colors.textMuted ?? "#a9afa9"
        font.pixelSize: 11
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }

    ToolButton {
        id: closeButton
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 5
        width: 34
        height: 34
        visible: root.popup
        text: "×"
        focusPolicy: Qt.TabFocus
        Accessible.name: qsTr("Hide notification popup")
        onClicked: root.presentation.closePopup(root.notificationId)
    }

    Text {
        id: summaryText
        objectName: "notificationSummary"
        anchors.left: appName.left
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.top: appName.bottom
        anchors.topMargin: 3
        text: root.summary
        color: root.colors.text ?? "#f2f1eb"
        font.pixelSize: 14
        font.bold: true
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }

    Text {
        objectName: "notificationBody"
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.top: identity.bottom
        anchors.topMargin: 8
        height: actionRow.visible ? 36 : 58
        text: root.body
        color: root.colors.text ?? "#f2f1eb"
        font.pixelSize: 12
        wrapMode: Text.Wrap
        elide: Text.ElideRight
        maximumLineCount: actionRow.visible ? 2 : 3
        textFormat: Text.PlainText
    }

    Row {
        id: actionRow
        objectName: "notificationActionRow"
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        spacing: 6
        visible: root.active

        Repeater {
            model: root.primaryActionCount

            Button {
                id: primaryActionButton
                required property var modelData
                readonly property var notificationAction:
                    root.actionAt(Number(modelData))
                height: 30
                width: 96
                text: String(notificationAction.label)
                focusPolicy: Qt.TabFocus
                Accessible.name: String(notificationAction.label)
                contentItem: Text {
                    text: primaryActionButton.text
                    color: primaryActionButton.palette.buttonText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
                onClicked: root.presentation.invokeAction(
                               root.notificationId,
                               String(notificationAction.key))
            }
        }

        Button {
            id: moreButton
            objectName: "notificationMoreActions"
            height: 30
            width: 64
            visible: root.actionCount > root.primaryActionCount
            text: qsTr("More")
            focusPolicy: Qt.TabFocus
            Accessible.name: qsTr("More notification actions")
            onClicked: overflowMenu.open()

            Menu {
                id: overflowMenu
                y: -height

                Repeater {
                    model: Math.max(0, root.actionCount - root.primaryActionCount)

                    MenuItem {
                        id: overflowActionItem
                        required property var modelData
                        readonly property var notificationAction:
                            root.actionAt(Number(modelData) +
                                          root.primaryActionCount)
                        width: 240
                        text: String(notificationAction.label)
                        Accessible.name: String(notificationAction.label)
                        contentItem: Text {
                            text: overflowActionItem.text
                            color: overflowActionItem.palette.buttonText
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                        onTriggered: root.presentation.invokeAction(
                                         root.notificationId,
                                         String(notificationAction.key))
                    }
                }
            }
        }

        Button {
            objectName: "notificationDismiss"
            height: 30
            width: 84
            visible: root.active
            text: qsTr("Dismiss")
            focusPolicy: Qt.TabFocus
            Accessible.name: qsTr("Dismiss notification")
            onClicked: root.presentation.dismiss(root.notificationId)
        }
    }
}
