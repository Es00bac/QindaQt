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
    required property Component notificationsComponent
    required property Component appearanceComponent
    property Component displayComponent: null
    property Component networkComponent: null
    required property Component unavailableComponent
    property bool presentationActive: true
    property string objectNamePrefix: "settingsRoute"
    property bool applicationClosePending: false
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
        return true
    }

    // AGENT-GUARD: A top-level close must remain rejected until the route's
    // existing discard dialog resolves the dirty draft. Loader construction is
    // asynchronous, so keep the request pending rather than accepting a close
    // while the active responsive host is still creating CustomizeRoute.
    function requestApplicationClose() {
        if (!host.presentationActive || !host.customizeSettings.dirty) {
            return false
        }
        host.applicationClosePending = true
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
            onCloseRequested: host.applicationClosePending = false
            onCloseCancelled: host.applicationClosePending = false
        }
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
                        && host.navigation.activeRouteComponent !== "customize"))
        sourceComponent: host.unavailableComponent
    }
}
