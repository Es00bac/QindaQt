// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

Column {
    id: root

    required property var panel
    required property var theme
    required property string zone
    property bool liveApplets: false
    property var notificationPresentation: null
    spacing: 4

    function appletZone(applet) {
        const settings = applet.settings ?? ({});
        return settings.zone ?? "start";
    }

    Repeater {
        model: root.panel.applets ?? []

        AppletChip {
            required property var modelData

            visible: root.appletZone(modelData) === root.zone
            width: root.width
            applet: modelData
            theme: root.theme
            vertical: true
            liveApplets: root.liveApplets
            notificationPresentation: root.notificationPresentation
        }
    }
}
