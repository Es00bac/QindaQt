// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Rectangle {
    id: root

    required property var applet
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property var settings: applet.settings ?? ({})
    readonly property string pluginId: String(applet.plugin)
    readonly property bool iconic: pluginId === "dock-task-list"

    width: vertical ? 50 : Math.max(42, label.implicitWidth + 18)
    height: vertical ? Math.max(42, label.implicitHeight + 16) : 28
    radius: Math.min(theme.cornerRadius ?? 8, 8)
    color: mouseArea.containsMouse ? colors.accent ?? "#8fc8b7"
          : settings.bare ? "transparent"
          : colors.surfaceRaised ?? "#2c312e"

    function displayLabel(plugin) {
        const labels = {
            "dock-task-list": "●  ◆  ■  ✦  ▣",
            "global-menu": "File   Edit   View   Window   Help",
            "system-menu": "Q",
            "system-status": "◉   Wi‑Fi   87%",
            "clock": "Tue 10:42",
            "launcher": "QindaQt"
        };
        return labels[plugin] ?? plugin.replace(/-/g, " ");
    }

    Text {
        id: label
        anchors.centerIn: parent
        width: vertical ? parent.width - 6 : implicitWidth
        text: root.displayLabel(root.pluginId)
        color: mouseArea.containsMouse ? root.colors.accentText ?? "#10201b"
                                       : root.colors.text ?? "white"
        font.pixelSize: root.vertical ? 9 : root.iconic ? 16 : 11
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
    }
}
