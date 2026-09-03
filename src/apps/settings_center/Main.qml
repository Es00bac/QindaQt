// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls
import QindaQt.SettingsApp.Appearance
import QindaQt.SettingsApp.Display
import QindaQt.SettingsApp.Network
import QindaQt.SettingsApp.Customize
import QindaQt.SettingsApp.Audio

T.ApplicationWindow {
    id: root

    required property var navigation
    required property var quietingSettings
    required property var appearanceSettings
    property var displaySettings: null
    property var networkSettings: null
    property var customizeSettings: CustomizeRouteComposition.model
    property bool applicationClosePending: false
    property var audioSettings: null

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

    onClosing: function(close) {
        const activeHost = root.isCompact ? compactRouteHost : wideRouteHost
        if (activeHost.requestApplicationClose()) {
            root.applicationClosePending = true
            close.accepted = false
        }
    }

    Shortcut {
        sequence: "Ctrl+1"
        onActivated: root.navigation.selectRoute("notifications")
    }

    Shortcut {
        sequence: "Ctrl+2"
        onActivated: root.navigation.selectRoute("appearance")
    }

    Shortcut {
        sequence: "Ctrl+3"
        onActivated: root.navigation.selectRoute("display")
    }

    Shortcut {
        sequence: "Ctrl+4"
        onActivated: root.navigation.selectRoute("network")
    }

    Shortcut {
        sequence: "Ctrl+6"
        onActivated: root.navigation.selectRoute("audio")
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

    // AGENT-GUARD: Keep the plural `sequences` spelling for StandardKey.Quit.
    // The singular `sequence` binds only one of the platform's multiple Quit
    // key bindings and emits a QML warning that aborts the QT_FATAL_WARNINGS
    // navigation-page test row during Main.qml construction.
    Shortcut {
        sequences: [StandardKey.Quit]
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
            displaySettings: root.displaySettings
            networkSettings: root.networkSettings
            customizeSettings: root.customizeSettings
            applicationClosePending: root.applicationClosePending
            audioSettings: root.audioSettings
            notificationsComponent: notificationsRouteComponent
            appearanceComponent: appearanceRouteComponent
            displayComponent: displayRouteComponent
            networkComponent: networkRouteComponent
            audioComponent: audioRouteComponent
            unavailableComponent: unavailableRouteComponent
            onApplicationCloseResolved: root.applicationClosePending = false
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
            displaySettings: root.displaySettings
            networkSettings: root.networkSettings
            customizeSettings: root.customizeSettings
            applicationClosePending: root.applicationClosePending
            audioSettings: root.audioSettings
            notificationsComponent: notificationsRouteComponent
            appearanceComponent: appearanceRouteComponent
            displayComponent: displayRouteComponent
            networkComponent: networkRouteComponent
            audioComponent: audioRouteComponent
            unavailableComponent: unavailableRouteComponent
            onApplicationCloseResolved: root.applicationClosePending = false
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
        id: displayRouteComponent
        DisplayPage {
            objectName: "displayPage"
            displaySettings: root.displaySettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: networkRouteComponent
        NetworkPage {
            objectName: "networkPage"
            networkSettings: root.networkSettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: audioRouteComponent
        AudioPage {
            objectName: "audioPage"
            audioSettings: root.audioSettings
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
