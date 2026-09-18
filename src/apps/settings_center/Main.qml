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
import QindaQt.SettingsApp.Accessibility
import QindaQt.SettingsApp.Input
import QindaQt.SettingsApp.InputBackend
import QindaQt.SettingsApp.Streaming
import QindaQt.SettingsApp.StreamingBackend
import QindaQt.SettingsApp.Windows
import QindaQt.SettingsApp.DefaultApplications
import QindaQt.SettingsApp.AboutComputer
import QindaQt.SettingsApp.Startup

T.ApplicationWindow {
    id: root

    required property var navigation
    required property var quietingSettings
    required property var quietingSchedule
    required property var appearanceSettings
    property var windowDecorationSettings: null
    property var displaySettings: null
    property var networkSettings: null
    property var customizeSettings: CustomizeRouteComposition.model
    property var audioSettings: null
    property var bluetoothSettings: null
    property var powerSettings: PowerRouteComposition.model
    property var screenLockSettings: PowerRouteComposition.screenLockSettings
    property var idleDisplaySettings: PowerRouteComposition.idleDisplaySettings
    property var clipboardSettings: ClipboardRouteComposition.model
    property var colorSettings: ColorRouteComposition.model
    property var accessibilitySettings: null
    property var windowsSettings: WindowsRouteComposition.model
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

    SettingsRouteShortcuts { navigation: root.navigation }

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
            screenLockSettings: root.screenLockSettings
            idleDisplaySettings: root.idleDisplaySettings
            clipboardSettings: root.clipboardSettings
            colorSettings: root.colorSettings
            accessibilitySettings: root.accessibilitySettings
            notificationsComponent: notificationsRouteComponent
            appearanceComponent: appearanceRouteComponent
            displayComponent: displayRouteComponent
            networkComponent: networkRouteComponent
            audioComponent: audioRouteComponent
            bluetoothComponent: bluetoothRouteComponent
            powerComponent: powerRouteComponent
            clipboardComponent: clipboardRouteComponent
            colorComponent: colorRouteComponent
            accessibilityComponent: accessibilityRouteComponent
            inputComponent: inputRouteComponent
            streamingComponent: streamingRouteComponent
            dateTimeComponent: addedRouteComponents.dateTime
            windowsComponent: windowsRouteComponent
            defaultApplicationsComponent: addedRouteComponents.defaultApplications
            aboutComputerComponent: addedRouteComponents.aboutComputer
            startupComponent: addedRouteComponents.startup
            startupComponent: addedRouteComponents.startup
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
            screenLockSettings: root.screenLockSettings
            idleDisplaySettings: root.idleDisplaySettings
            clipboardSettings: root.clipboardSettings
            colorSettings: root.colorSettings
            accessibilitySettings: root.accessibilitySettings
            notificationsComponent: notificationsRouteComponent
            appearanceComponent: appearanceRouteComponent
            displayComponent: displayRouteComponent
            networkComponent: networkRouteComponent
            audioComponent: audioRouteComponent
            bluetoothComponent: bluetoothRouteComponent
            powerComponent: powerRouteComponent
            clipboardComponent: clipboardRouteComponent
            colorComponent: colorRouteComponent
            accessibilityComponent: accessibilityRouteComponent
            inputComponent: inputRouteComponent
            streamingComponent: streamingRouteComponent
            dateTimeComponent: addedRouteComponents.dateTime
            windowsComponent: windowsRouteComponent
            defaultApplicationsComponent: addedRouteComponents.defaultApplications
            aboutComputerComponent: addedRouteComponents.aboutComputer
            startupComponent: addedRouteComponents.startup
            startupComponent: addedRouteComponents.startup
            unavailableComponent: unavailableRouteComponent
            onApplicationCloseResolved: root.applicationClosePending = false
        }
    }

    AddedRouteComponents {
        id: addedRouteComponents
        onCloseRequested: root.close()
    }

    Component {
        id: notificationsRouteComponent
        NotificationsPage {
            objectName: "notificationsPage"
            quietingSettings: root.quietingSettings
            quietingSchedule: root.quietingSchedule
        }
    }

    Component {
        id: appearanceRouteComponent
        AppearancePage {
            objectName: "appearancePage"
            appearanceSettings: root.appearanceSettings
            windowDecorationSettings: root.windowDecorationSettings
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
            onPenSettingsRequested: root.navigation.selectRouteDestination(
                                        "input", "tablet")
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
            screenLockSettings: root.screenLockSettings
            idleDisplaySettings: root.idleDisplaySettings
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
        id: accessibilityRouteComponent
        AccessibilityPage {
            objectName: "accessibilityPage"
            accessibilitySettings: root.accessibilitySettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: inputRouteComponent
        InputPage {
            objectName: "inputPage"
            inputSettings: InputRouteComposition
            initialDestination: root.navigation.requestedDestination
            initialSelection: root.navigation.requestedSelection
            onCloseRequested: root.close()
        }
    }

    Component {
        id: streamingRouteComponent
        StreamingPage {
            objectName: "streamingPage"
            streamingSettings: StreamingRouteComposition.streaming
            onCloseRequested: root.close()
        }
    }

    Component {
        id: windowsRouteComponent
        WindowsPage {
            objectName: "windowsPage"
            windowsSettings: root.windowsSettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: unavailableRouteComponent
        SettingsUnavailableRoute { navigation: root.navigation }
    }
}
