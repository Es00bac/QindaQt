// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Rectangle {
    id: root

    required property var applet
    required property var theme
    property bool vertical: false
    property bool liveApplets: false
    property var notificationCenterAppletAccess: null
    readonly property var colors: theme.colors ?? ({})
    readonly property var settings: applet.settings ?? ({})
    readonly property var runtime: applet.runtime ?? ({})
    readonly property string pluginId: String(applet.plugin)
    readonly property bool iconic: pluginId === "dock-task-list"
    readonly property bool usesLiveContent: builtinContent.hasLiveContent
    readonly property bool runtimeUnavailable: liveApplets && runtime.ready !== true

    width: vertical ? 50 : Math.max(42, usesLiveContent
                                         ? builtinContent.implicitWidth
                                         : label.implicitWidth + 18)
    height: vertical ? Math.max(42, usesLiveContent
                                     ? builtinContent.implicitHeight
                                     : label.implicitHeight + 16) : 28
    radius: Math.min(theme.cornerRadius ?? 8, 8)
    color: builtinContent.selected || hoverHandler.hovered
          ? colors.accent ?? "#8fc8b7"
          : settings.bare ? "transparent"
          : colors.surfaceRaised ?? "#2c312e"

    function displayLabel(plugin) {
        const labels = {
            "dock-task-list": "●  ◆  ■  ✦  ▣",
            "global-menu": "File   Edit   View   Window   Help",
            "system-menu": "Q",
            "system-status": "◉   Wi‑Fi   87%",
            "clock": "Tue 10:42",
            "notification-center": "Notifications",
            "launcher": "QindaQt"
        };
        return labels[plugin] ?? plugin.replace(/-/g, " ");
    }

    Text {
        id: label
        anchors.centerIn: parent
        width: root.vertical ? parent.width - 6 : implicitWidth
        text: root.displayLabel(root.pluginId)
        visible: !root.usesLiveContent
        color: hoverHandler.hovered ? root.colors.accentText ?? "#10201b"
                                    : root.colors.text ?? "white"
        font.pixelSize: root.vertical ? 9 : root.iconic ? 16 : 11
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
    }

    BuiltinAppletContent {
        id: builtinContent
        anchors.fill: parent
        visible: root.usesLiveContent
        applet: root.applet
        theme: root.theme
        vertical: root.vertical
        liveApplets: root.liveApplets
        notificationCenterAppletAccess: root.notificationCenterAppletAccess
    }

    Rectangle {
        // AGENT-NOTE: an unavailable plug-in stays visible as profile content,
        // but the amber marker prevents a static mock from claiming to be live.
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 3
        width: 6
        height: 6
        radius: 3
        visible: root.runtimeUnavailable
        color: root.colors.warning ?? "#e5a84b"
        border.color: root.colors.surface ?? "#222624"
        border.width: 1
    }

    HoverHandler {
        id: hoverHandler
    }
}
