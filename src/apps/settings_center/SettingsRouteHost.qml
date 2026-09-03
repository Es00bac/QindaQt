// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: host

    required property var navigation
    required property var quietingSettings
    required property var appearanceSettings
    property var displaySettings: null
    property var networkSettings: null
    property var audioSettings: null
    required property Component notificationsComponent
    required property Component appearanceComponent
    property Component displayComponent: null
    property Component networkComponent: null
    property Component audioComponent: null
    required property Component unavailableComponent
    property bool presentationActive: true
    property string objectNamePrefix: "settingsRoute"

    readonly property Loader currentLoader: !navigation.activeRouteAvailable
        ? unavailableLoader
        : navigation.activeRouteComponent === "notifications"
          ? notificationsLoader
          : navigation.activeRouteComponent === "appearance"
            ? appearanceLoader
            : navigation.activeRouteComponent === "display"
              ? displayLoader
            : navigation.activeRouteComponent === "network"
              ? networkLoader
            : navigation.activeRouteComponent === "audio"
              ? audioLoader
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

    Loader {
        id: notificationsLoader
        objectName: host.objectNamePrefix + "NotificationsLoader"
        anchors.fill: parent
        active: host.presentationActive
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "notifications"
        sourceComponent: host.notificationsComponent
    }

    Loader {
        id: appearanceLoader
        objectName: host.objectNamePrefix + "AppearanceLoader"
        anchors.fill: parent
        active: host.presentationActive
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "appearance"
        sourceComponent: host.appearanceComponent
    }

    Loader {
        id: displayLoader
        objectName: host.objectNamePrefix + "DisplayLoader"
        anchors.fill: parent
        active: host.presentationActive
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
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "network"
                && host.networkComponent !== null
        sourceComponent: host.networkComponent
    }

    Loader {
        id: audioLoader
        objectName: host.objectNamePrefix + "AudioLoader"
        anchors.fill: parent
        active: host.presentationActive
                && host.navigation.activeRouteAvailable
                && host.navigation.activeRouteComponent === "audio"
                && host.audioComponent !== null
        sourceComponent: host.audioComponent
    }

    Loader {
        id: unavailableLoader
        objectName: host.objectNamePrefix + "UnavailableLoader"
        anchors.fill: parent
        // An unrecognized component key is presentation-hostile even if a
        // malformed producer claimed the route was otherwise available.
        active: host.presentationActive
                && (!host.navigation.activeRouteAvailable
                    || (host.navigation.activeRouteComponent !== "notifications"
                        && host.navigation.activeRouteComponent !== "appearance"
                        && (host.navigation.activeRouteComponent !== "display" || host.displayComponent === null)
                        && (host.navigation.activeRouteComponent !== "network" || host.networkComponent === null)
                        && (host.navigation.activeRouteComponent !== "audio" || host.audioComponent === null)))
        sourceComponent: host.unavailableComponent
    }
}
