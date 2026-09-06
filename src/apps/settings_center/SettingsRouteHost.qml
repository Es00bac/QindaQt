// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.SettingsApp.Customize

Item {
    id: host

    required property var navigation
    required property var quietingSettings
    required property var appearanceSettings
    property var displaySettings: null
    property var networkSettings: null
    required property var customizeSettings
    property var audioSettings: null
    property var bluetoothSettings: null
    property var powerSettings: null
    property var screenLockSettings: null
    property var clipboardSettings: null
    property var colorSettings: null
    required property Component notificationsComponent
    required property Component appearanceComponent
    property Component displayComponent: null
    property Component networkComponent: null
    property Component audioComponent: null
    property Component bluetoothComponent: null
    property Component powerComponent: null
    property Component clipboardComponent: null
    property Component colorComponent: null
    required property Component unavailableComponent
    property bool presentationActive: true
    property string objectNamePrefix: "settingsRoute"
    required property bool applicationClosePending
    signal applicationCloseResolved()
    readonly property bool customizeDeparturePending:
        host.presentationActive && host.customizeSettings.dirty
        && host.navigation.activeRouteComponent !== "customize"

    readonly property Loader currentLoader: customizeDeparturePending
        ? customizeLoader
        : !navigation.activeRouteAvailable
        ? unavailableLoader
        : navigation.activeRouteComponent === "notifications"
          ? notificationsLoader
          : navigation.activeRouteComponent === "appearance"
            ? appearanceLoader
            : navigation.activeRouteComponent === "display"
              ? displayLoader
            : navigation.activeRouteComponent === "network"
              ? networkLoader
            : navigation.activeRouteComponent === "customize"
              ? customizeLoader
            : navigation.activeRouteComponent === "audio"
              ? audioLoader
            : navigation.activeRouteComponent === "bluetooth"
              ? bluetoothLoader
            : navigation.activeRouteComponent === "power"
              ? powerLoader
            : navigation.activeRouteComponent === "clipboard"
              ? clipboardLoader
            : navigation.activeRouteComponent === "color"
              ? colorLoader
              : unavailableLoader

    // AGENT-CONTRACT: Exactly one host is presentation-active at a time. The
    // compact and wide shells may coexist for responsive layout, but inactive
    // hosts must not duplicate route pages or their focus and settings bindings.
    function focusCurrentContent() {
        const page = currentLoader.item
        if (page === null || page === undefined) {
            return false
        }
        const target = page.firstFocusTarget !== undefined
            ? page.firstFocusTarget
            : page.firstThemeCard !== undefined ? page.firstThemeCard : page
        if (target === null || target === undefined) {
            return false
        }
        target.forceActiveFocus(Qt.TabFocusReason)
        // AGENT-NOTE: callers (route tab KeyNavigation, Escape-return focus)
        // and tests rely on this reporting whether focus actually moved.
        // forceActiveFocus() returns void; confirm the item holds active
        // focus so a disabled or not-yet-focusable target is reported as a
        // failure instead of a silent success.
        return target.activeFocus
    }

    // AGENT-GUARD: Main.qml owns applicationClosePending because responsive
    // reconstruction destroys this host. The active host must keep loading the
    // dialog until Cancel or Discard resolves that shared close decision.
    function requestApplicationClose() {
        if (!host.presentationActive || !host.customizeSettings.dirty) {
            return false
        }
        if (customizeLoader.item !== null) {
            customizeLoader.item.requestClose()
        }
        return true
    }

    Loader {
        id: notificationsLoader
        objectName: host.objectNamePrefix + "NotificationsLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "notifications"
        sourceComponent: host.notificationsComponent
    }

    Loader {
        id: appearanceLoader
        objectName: host.objectNamePrefix + "AppearanceLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "appearance"
        sourceComponent: host.appearanceComponent
    }

    Loader {
        id: displayLoader
        objectName: host.objectNamePrefix + "DisplayLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "display"
                && host.displayComponent !== null
        sourceComponent: host.displayComponent
    }

    Loader {
        id: networkLoader
        objectName: host.objectNamePrefix + "NetworkLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "network"
                && host.networkComponent !== null
        sourceComponent: host.networkComponent
    }

    Loader {
        id: customizeLoader
        objectName: host.objectNamePrefix + "CustomizeLoader"
        anchors.fill: parent
        active: host.presentationActive
                && ((host.navigation.activeRouteAvailable
                     && host.navigation.activeRouteComponent === "customize")
                    || host.customizeDeparturePending
                    || host.applicationClosePending)
        sourceComponent: customizeRouteComponent
        onLoaded: {
            if (host.customizeDeparturePending
                    || host.applicationClosePending) {
                item.requestClose()
            }
        }
    }

    Component {
        id: customizeRouteComponent
        CustomizeRoute {
            customizeSettings: host.customizeSettings
            navigation: host.navigation
            onCloseRequested: host.applicationCloseResolved()
            onCloseCancelled: host.applicationCloseResolved()
        }
    }

    Loader {
        id: audioLoader
        objectName: host.objectNamePrefix + "AudioLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "audio"
                && host.audioComponent !== null
        sourceComponent: host.audioComponent
    }

    Loader {
        id: bluetoothLoader
        objectName: host.objectNamePrefix + "BluetoothLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "bluetooth"
                && host.bluetoothComponent !== null
        sourceComponent: host.bluetoothComponent
    }

    Loader {
        id: powerLoader
        objectName: host.objectNamePrefix + "PowerLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "power"
                && host.powerComponent !== null
        sourceComponent: host.powerComponent
    }

    Loader {
        id: clipboardLoader
        objectName: host.objectNamePrefix + "ClipboardLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "clipboard"
                && host.clipboardComponent !== null
        sourceComponent: host.clipboardComponent
    }

    Loader {
        id: colorLoader
        objectName: host.objectNamePrefix + "ColorLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "color"
                && host.colorComponent !== null
        sourceComponent: host.colorComponent
    }

    Loader {
        id: unavailableLoader
        objectName: host.objectNamePrefix + "UnavailableLoader"
        anchors.fill: parent
        // An unrecognized component key is presentation-hostile even if a
        // malformed producer claimed the route was otherwise available.
        active: host.presentationActive
                && !host.customizeDeparturePending
                && (!host.navigation.activeRouteAvailable
                    || (host.navigation.activeRouteComponent !== "notifications"
                        && host.navigation.activeRouteComponent !== "appearance"
                        && (host.navigation.activeRouteComponent !== "display" || host.displayComponent === null)
                        && (host.navigation.activeRouteComponent !== "network" || host.networkComponent === null)
                        && host.navigation.activeRouteComponent !== "customize"
                        && (host.navigation.activeRouteComponent !== "audio" || host.audioComponent === null)
                        && (host.navigation.activeRouteComponent !== "bluetooth" || host.bluetoothComponent === null)
                        && (host.navigation.activeRouteComponent !== "power" || host.powerComponent === null)
                        && (host.navigation.activeRouteComponent !== "clipboard" || host.clipboardComponent === null)
                        && (host.navigation.activeRouteComponent !== "color" || host.colorComponent === null)))
        sourceComponent: host.unavailableComponent
    }
}
