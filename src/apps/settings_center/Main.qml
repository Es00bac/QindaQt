// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls
import QindaQt.SettingsApp.Appearance

T.ApplicationWindow {
    id: root

    required property var navigation
    required property var quietingSettings
    required property var appearanceSettings

    readonly property bool isCompact: width < 540
    readonly property string currentRouteTitle: navigation.activeRouteTitle.length > 0
        ? navigation.activeRouteTitle : qsTr("Unavailable page")

    visible: true
    width: 720
    height: 520
    minimumWidth: 420
    minimumHeight: 320
    color: Tokens.bg.base
    title: qsTr("QindaQt Settings — %1").arg(currentRouteTitle)

    Shortcut {
        sequence: "Ctrl+1"
        onActivated: root.navigation.selectRoute("notifications")
    }

    Shortcut {
        sequence: "Ctrl+2"
        onActivated: root.navigation.selectRoute("appearance")
    }

    Shortcut {
        sequence: "Alt+Left"
        onActivated: {
            if (root.navigation.previousRouteId.length > 0) {
                root.navigation.selectRoute(root.navigation.previousRouteId)
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: root.isCompact ? compactHeader.focusActiveButton()
                                    : sidebar.focusActiveButton()
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: root.close()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: !root.isCompact

        SettingsSidebar {
            id: sidebar
            objectName: "settingsSidebar"
            Layout.fillHeight: true
            Layout.preferredWidth: 200
            navigation: root.navigation
            onContentFocusRequested: wideRouteHost.focusCurrentContent()
        }

        SettingsRouteHost {
            id: wideRouteHost
            objectName: "wideSettingsRouteHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            presentationActive: !root.isCompact
            objectNamePrefix: "wideSettingsRoute"
            navigation: root.navigation
            quietingSettings: root.quietingSettings
            appearanceSettings: root.appearanceSettings
            notificationsComponent: notificationsRouteComponent
            appearanceComponent: appearanceRouteComponent
            unavailableComponent: unavailableRouteComponent
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: root.isCompact

        SettingsCompactHeader {
            id: compactHeader
            objectName: "settingsCompactHeader"
            Layout.fillWidth: true
            navigation: root.navigation
            onContentFocusRequested: compactRouteHost.focusCurrentContent()
        }

        SettingsRouteHost {
            id: compactRouteHost
            objectName: "compactSettingsRouteHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            presentationActive: root.isCompact
            objectNamePrefix: "compactSettingsRoute"
            navigation: root.navigation
            quietingSettings: root.quietingSettings
            appearanceSettings: root.appearanceSettings
            notificationsComponent: notificationsRouteComponent
            appearanceComponent: appearanceRouteComponent
            unavailableComponent: unavailableRouteComponent
        }
    }

    Component {
        id: notificationsRouteComponent
        NotificationsPage {
            objectName: "notificationsPage"
            quietingSettings: root.quietingSettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: appearanceRouteComponent
        AppearancePage {
            objectName: "appearancePage"
            appearanceSettings: root.appearanceSettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: unavailableRouteComponent
        Item {
            readonly property Item firstFocusTarget: unavailableNotice

            Controls.DegradedNotice {
                id: unavailableNotice
                objectName: "settingsUnavailableNotice"
                anchors.centerIn: parent
                width: Math.min(parent.width - Tokens.space["6"] * 2, 380)
                reason: root.navigation.activeRouteUnavailableReason.length > 0
                    ? root.navigation.activeRouteUnavailableReason
                    : qsTr("This settings page is unavailable.")
            }
        }
    }
}
