// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QindaQt.SettingsApp.Appearance

ApplicationWindow {
    id: root
    required property var quietingSettings
    property var appearanceSettings: undefined
    required property string route
    visible: true
    width: 560
    height: 420
    minimumWidth: 420
    minimumHeight: 320
    title: route === "appearance"
               ? qsTr("QindaQt Settings — Appearance")
               : qsTr("QindaQt Settings — Notifications")

    // Route seam: each first-party settings route is one component with its
    // own required model property, instantiated only for its route so a
    // missing model can never silently bind to the wrong domain.
    Loader {
        anchors.fill: parent
        active: root.route === "appearance"
        sourceComponent: appearanceRouteComponent
    }

    Loader {
        anchors.fill: parent
        active: root.route !== "appearance"
        sourceComponent: notificationsRouteComponent
    }

    Component {
        id: notificationsRouteComponent
        NotificationsPage {
            quietingSettings: root.quietingSettings
            onCloseRequested: root.close()
        }
    }

    Component {
        id: appearanceRouteComponent
        AppearancePage {
            appearanceSettings: root.appearanceSettings
            onCloseRequested: root.close()
        }
    }
}
