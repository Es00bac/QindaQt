// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: host

    required property var navigation
    required property var quietingSettings
    required property var appearanceSettings
    required property Component notificationsComponent
    required property Component appearanceComponent
    required property Component unavailableComponent
    property bool presentationActive: true
    property string objectNamePrefix: "settingsRoute"

    readonly property Loader currentLoader: !navigation.activeRouteAvailable
        ? unavailableLoader
        : navigation.activeRouteComponent === "notifications"
          ? notificationsLoader
          : navigation.activeRouteComponent === "appearance"
            ? appearanceLoader
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
        id: unavailableLoader
        objectName: host.objectNamePrefix + "UnavailableLoader"
        anchors.fill: parent
        // An unrecognized component key is presentation-hostile even if a
        // malformed producer claimed the route was otherwise available.
        active: host.presentationActive
                && (!host.navigation.activeRouteAvailable
                    || (host.navigation.activeRouteComponent !== "notifications"
                        && host.navigation.activeRouteComponent !== "appearance"))
        sourceComponent: host.unavailableComponent
    }
}
