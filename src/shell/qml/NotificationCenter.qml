// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Window {
    id: root

    required property var presentation
    required property var quietingSettings
    required property var settingsLauncher
    required property var theme
    readonly property var colors: theme.colors ?? ({})

    visible: false
    color: "transparent"
    title: qsTr("Notification center")
    flags: Qt.FramelessWindowHint

    function seedInitialFocus() {
        // AGENT-GUARD: Wayland activation can install an unnamed content item
        // after activeChanged. Seed on the next event-loop turn, but never
        // replace a named control that the user has already reached.
        if (active && (activeFocusItem === null
                       || activeFocusItem.objectName.length === 0))
            closeButton.forceActiveFocus(Qt.ActiveWindowFocusReason)
    }

    onActiveChanged: if (active) Qt.callLater(seedInitialFocus)

    Shortcut {
        objectName: "notificationCenterCloseShortcut"
        sequence: "Escape"
        context: Qt.WindowShortcut
        enabled: root.visible
        onActivated: root.presentation.centerOpen = false
    }

    Rectangle {
        anchors.fill: parent
        radius: Math.min(root.theme.cornerRadius ?? 10, 16)
        color: root.colors.surface ?? "#222624"
        border.color: root.colors.border ?? "#3c433f"
        border.width: 1
        // The focused Quick control receives injected/physical keys before a
        // WindowShortcut is considered on every platform. Handle Escape on
        // their common content ancestor so closing remains window-scoped.
        Keys.onEscapePressed: root.presentation.centerOpen = false

        Text {
            id: titleText
            objectName: "notificationCenterTitle"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16
            text: qsTr("Notifications")
            color: root.colors.text ?? "#f2f1eb"
            font.pixelSize: 20
            font.bold: true
            textFormat: Text.PlainText
        }

        ToolButton {
            id: doNotDisturbButton
            objectName: "notificationDoNotDisturbButton"
            anchors.right: clearButton.left
            anchors.rightMargin: 6
            anchors.verticalCenter: titleText.verticalCenter
            width: 40
            height: 32
            text: "☾"
            checkable: true
            checked: root.quietingSettings.enabled
            enabled: root.quietingSettings.canToggle
            focusPolicy: Qt.TabFocus
            Accessible.role: Accessible.CheckBox
            Accessible.name: checked ? qsTr("Turn off Do Not Disturb")
                                     : qsTr("Turn on Do Not Disturb")
            Accessible.description: qsTr(
                "Low and normal notification banners are hidden; critical banners remain visible")
            ToolTip.visible: hovered
            ToolTip.text: checked
                          ? qsTr("Do Not Disturb is on; critical banners remain visible")
                          : qsTr("Hide low and normal notification banners")
            onClicked: root.quietingSettings.requestSet(
                           !root.quietingSettings.enabled)
        }

        Button {
            id: clearButton
            objectName: "notificationClearHistoryButton"
            anchors.right: closeButton.left
            anchors.rightMargin: 6
            anchors.verticalCenter: titleText.verticalCenter
            text: qsTr("Clear history")
            enabled: historySection.count > 0
            focusPolicy: Qt.TabFocus
            Accessible.role: Accessible.Button
            onClicked: root.presentation.clearHistory()
        }

        NotificationOperationStatus {
            anchors.left: titleText.right
            anchors.leftMargin: 12
            anchors.right: doNotDisturbButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: titleText.verticalCenter
            presentation: root.presentation
            theme: root.theme
        }

        ToolButton {
            id: closeButton
            objectName: "notificationCenterCloseButton"
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            width: 36
            height: 36
            text: "×"
            focusPolicy: Qt.TabFocus
            // AGENT-GUARD: Keep traversal on Qt Quick's complete natural tab
            // chain. Header-only KeyNavigation edges trap focus away from the
            // dynamically instantiated card, retry, and settings controls.
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Close notification center")
            onClicked: root.presentation.centerOpen = false
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
            anchors.bottom: quietingStatus.visible ? quietingStatus.top : footerRow.top
            anchors.margins: 12
            heading: qsTr("Recent")
            notificationModel: root.presentation.historyModel
            presentation: root.presentation
            theme: root.theme
        }

        Text {
            id: quietingStatus
            objectName: "notificationQuietingStatus"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footerRow.top
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 4
            visible: text.length > 0
            text: root.quietingSettings.statusText.length > 0
                  ? root.quietingSettings.statusText
                  : root.quietingSettings.errorText
            color: root.quietingSettings.conflict
                   || root.quietingSettings.unavailable
                   || root.quietingSettings.errorText.length > 0
                   ? (root.colors.danger ?? "#f07c76")
                   : (root.colors.textMuted ?? "#a9afa9")
            elide: Text.ElideRight
            textFormat: Text.PlainText
            Accessible.role: root.quietingSettings.conflict
                             || root.quietingSettings.unavailable
                             || root.quietingSettings.errorText.length > 0
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
        }

        Row {
            id: footerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 8

            Button {
                id: stateAction
                objectName: "notificationQuietingStateAction"
                visible: root.quietingSettings.conflict
                         || root.quietingSettings.unavailable
                text: root.quietingSettings.conflict
                      ? qsTr("Apply my choice") : qsTr("Retry")
                focusPolicy: Qt.TabFocus
                onClicked: {
                    if (root.quietingSettings.conflict)
                        root.quietingSettings.applyMyChoice()
                    else
                        root.quietingSettings.retry()
                }
            }

            Button {
                id: settingsButton
                objectName: "notificationSettingsRouteButton"
                text: qsTr("Notification settings…")
                focusPolicy: Qt.TabFocus
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Notification settings")
                onClicked: root.settingsLauncher.openNotifications()
            }
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
