// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Row {
    id: root

    required property var panel
    required property var theme
    required property string zone
    property bool liveApplets: false
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
            height: root.height
            applet: modelData
            theme: root.theme
            liveApplets: root.liveApplets
        }
    }
}
