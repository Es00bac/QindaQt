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
import QindaQt.SettingsApp.Bluetooth
import QindaQt.SettingsApp.Power
import QindaQt.SettingsApp.PowerBackend
import QindaQt.SettingsApp.Clipboard
import QindaQt.SettingsApp.Color
import QindaQt.SettingsApp.ColorBackend

T.ApplicationWindow {
    id: root

    required property var navigation
    required property var quietingSettings
    required property var appearanceSettings
    property var displaySettings: null
    property var networkSettings: null
    property var customizeSettings: CustomizeRouteComposition.model
    property var audioSettings: null
    property var bluetoothSettings: null
    property var powerSettings: PowerRouteComposition.model
    property var clipboardSettings: ClipboardRouteComposition.model
    property var colorSettings: ColorRouteComposition.model
    property bool applicationClosePending: false
    property bool bluetoothClosePending: false

    readonly property bool isCompact: width < 540
    readonly property string currentRouteTitle: navigation.activeRouteTitle.length > 0
        ? navigation.activeRouteTitle : qsTr("Unavailable page")

    visible: true
    width: 960
    height: 680
    minimumWidth: 420
    minimumHeight: 320
    color: Tokens.bg.base
    title: qsTr("QindaQt Settings — %1").arg(currentRouteTitle)

    onClosing: function(close) {
        if (root.bluetoothSettings !== null)
            root.bluetoothSettings.setRouteActive(false)
        const activeHost = root.isCompact ? compactRouteHost : wideRouteHost
        const bluetoothWaiting = root.bluetoothSettings !== null
                && root.bluetoothSettings.departureReleasePending
        // AGENT-GUARD: Resolve the discovery release first. Customize may
        // complete its discard flow with Qt.quit(), which would bypass this
        // window-close fence if both decisions were opened concurrently.
        const customizeWaiting = bluetoothWaiting
                ? false : activeHost.requestApplicationClose()
        if (bluetoothWaiting)
            root.bluetoothClosePending = true
        if (customizeWaiting)
            root.applicationClosePending = true
        if (bluetoothWaiting || customizeWaiting)
            close.accepted = false
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
        sequence: "Ctrl+5"
        onActivated: root.navigation.selectRoute("customize")
    }

    Shortcut {
        sequence: "Ctrl+6"
        onActivated: root.navigation.selectRoute("audio")
    }

    Shortcut {
        sequence: "Ctrl+7"
        onActivated: root.navigation.selectRoute("bluetooth")
    }

    Shortcut {
        sequence: "Ctrl+8"
        onActivated: root.navigation.selectRoute("power")
    }

    Shortcut {
        sequence: "Ctrl+9"
        onActivated: root.navigation.selectRoute("clipboard")
    }

    Shortcut {
        sequence: "Ctrl+0"
        onActivated: root.navigation.selectRoute("color")
    }

    Component.onCompleted: {
        if (root.bluetoothSettings !== null)
            root.bluetoothSettings.setRouteActive(
                        root.navigation.activeRouteComponent === "bluetooth")
        if (root.colorSettings !== null)
            root.colorSettings.setRouteActive(
                        root.navigation.activeRouteComponent === "color")
    }

    Connections {
        target: root.navigation
        function onActiveRouteChanged() {
            if (root.bluetoothSettings !== null)
                root.bluetoothSettings.setRouteActive(
                            root.navigation.activeRouteComponent === "bluetooth")
            if (root.colorSettings !== null)
                root.colorSettings.setRouteActive(
                            root.navigation.activeRouteComponent === "color")
        }
    }

    Connections {
        target: root.bluetoothSettings
        enabled: root.bluetoothSettings !== null
        function onViewChanged() {
            if (root.bluetoothClosePending
                    && !root.bluetoothSettings.departureReleasePending) {
                root.bluetoothClosePending = false
                if (!root.applicationClosePending)
                    Qt.callLater(root.close)
            }
        }
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
        // AGENT-GUARD: two enabled window-context shortcuts with the same
        // sequence are ambiguous and Qt activates neither. While the
        // Bluetooth route shows an active prompt with a free reply lane, the
        // route's own Escape shortcut (BluetoothPairingSection.qml) must be
        // the only enabled match so the prompt receives its cancel reply.
        // This host shortcut yields then and stays enabled for every other
        // route or prompt state.
        enabled: !(root.navigation.activeRouteComponent === "bluetooth"
                   && root.bluetoothSettings !== null
                   && root.bluetoothSettings.pairingPrompt.active === true
                   && root.bluetoothSettings.pairingReplyPending !== true)
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
            bluetoothSettings: root.bluetoothSettings
            powerSettings: root.powerSettings
            clipboardSettings: root.clipboardSettings
            colorSettings: root.colorSettings
            notificationsComponent: notificationsRouteComponent
            appearanceComponent: appearanceRouteComponent
            displayComponent: displayRouteComponent
            networkComponent: networkRouteComponent
            audioComponent: audioRouteComponent
            bluetoothComponent: bluetoothRouteComponent
            powerComponent: powerRouteComponent
            clipboardComponent: clipboardRouteComponent
            colorComponent: colorRouteComponent
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
            bluetoothSettings: root.bluetoothSettings
            powerSettings: root.powerSettings
            clipboardSettings: root.clipboardSettings
            colorSettings: root.colorSettings
            notificationsComponent: notificationsRouteComponent
            appearanceComponent: appearanceRouteComponent
            displayComponent: displayRouteComponent
            networkComponent: networkRouteComponent
            audioComponent: audioRouteComponent
            bluetoothComponent: bluetoothRouteComponent
            powerComponent: powerRouteComponent
            clipboardComponent: clipboardRouteComponent
            colorComponent: colorRouteComponent
            unavailableComponent: unavailableRouteComponent
            onApplicationCloseResolved: root.applicationClosePending = false
        }
    }

    Component {
        id: notificationsRouteComponent
        NotificationsPage {
            objectName: "notificationsPage"
            quietingSettings: root.quietingSettings
        }
    }

    Component {
        id: appearanceRouteComponent
        AppearancePage {
            objectName: "appearancePage"
            appearanceSettings: root.appearanceSettings
            navigation: root.navigation
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
        id: bluetoothRouteComponent
        BluetoothPage {
            objectName: "bluetoothPage"
            bluetoothSettings: root.bluetoothSettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: powerRouteComponent
        PowerPage {
            objectName: "powerPage"
            powerSettings: root.powerSettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: clipboardRouteComponent
        ClipboardPage {
            objectName: "clipboardPage"
            clipboardSettings: root.clipboardSettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: colorRouteComponent
        ColorPage {
            objectName: "colorPage"
            colorSettings: root.colorSettings
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
