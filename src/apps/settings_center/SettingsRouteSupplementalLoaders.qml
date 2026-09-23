// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Keep later independent routes together while the host owns selection and
// the single active-Loader invariant. These aliases preserve its public
// Loader identity and object names for navigation and construction probes.
Item {
    id: supplemental
    required property var host
    property alias defaultApplicationsLoader: defaultApplicationsLoader
    property alias aboutComputerLoader: aboutComputerLoader
    property alias startupLoader: startupLoader
    property alias screensaverLoader: screensaverLoader
    property alias loginScreenLoader: loginScreenLoader
    property alias voiceLoader: voiceLoader

    Loader {
        id: defaultApplicationsLoader
        objectName: host.objectNamePrefix + "DefaultApplicationsLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation?.activeRouteAvailable
                && host.navigation?.activeRouteComponent === "default-apps"
                && host.defaultApplicationsComponent !== null
        sourceComponent: host.defaultApplicationsComponent
    }

    Loader {
        id: aboutComputerLoader
        objectName: host.objectNamePrefix + "AboutComputerLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation?.activeRouteAvailable
                && host.navigation?.activeRouteComponent === "about-computer"
                && host.aboutComputerComponent !== null
        sourceComponent: host.aboutComputerComponent
    }

    Loader {
        id: startupLoader
        objectName: host.objectNamePrefix + "StartupLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation?.activeRouteAvailable
                && host.navigation?.activeRouteComponent === "startup"
                && host.startupComponent !== null
        sourceComponent: host.startupComponent
    }

    Loader {
        id: screensaverLoader
        objectName: host.objectNamePrefix + "ScreensaverLoader"
        anchors.fill: parent
        // AGENT-NOTE: The Screen saver page takes its models from the
        // ScreensaverRouteComposition backend singleton, which constructs
        // without touching Settings1 or the locker configuration and
        // presents degraded truth itself when either is unreachable
        // (ADR-0226).
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation?.activeRouteAvailable
                && host.navigation?.activeRouteComponent === "screensaver"
                && host.screensaverComponent !== null
        sourceComponent: host.screensaverComponent
    }

    Loader {
        id: loginScreenLoader
        objectName: host.objectNamePrefix + "LoginScreenLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation?.activeRouteAvailable
                && host.navigation?.activeRouteComponent === "login-screen"
                && host.loginScreenComponent !== null
        sourceComponent: host.loginScreenComponent
    }

    Loader {
        id: voiceLoader
        objectName: host.objectNamePrefix + "VoiceLoader"
        anchors.fill: parent
        active: host.presentationActive
                && !host.customizeDeparturePending
                && host.navigation?.activeRouteAvailable
                && host.navigation?.activeRouteComponent === "voice"
                && host.voiceComponent !== null
        sourceComponent: host.voiceComponent
    }

}
