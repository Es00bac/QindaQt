// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root

    objectName: "clockApplet"

    required property var applet
    required property var theme
    property bool vertical: false
    property date currentTime: new Date()
    readonly property var settings: applet.settings ?? ({})
    readonly property var colors: theme.colors ?? ({})
    readonly property bool showSeconds: settings.showSeconds ?? false
    readonly property bool showDate: settings.showDate ?? true
    // Worn Luna dressing (ADR-0124): the Bliss profile opts this instance in
    // through its own settings, and the Luna taskbar dispatcher sets it for
    // any clock it hosts; every other profile keeps the token colors.
    property bool luna: settings.presentation === "luna"

    implicitWidth: Math.max(48, label.implicitWidth + 12)
    implicitHeight: Math.max(24, label.implicitHeight + 6)

    function timeFormat() {
        if (settings.format === "12-hour")
            return showSeconds ? "h:mm:ss AP" : "h:mm AP";
        if (settings.format === "24-hour")
            return showSeconds ? "HH:mm:ss" : "HH:mm";
        return Qt.locale().timeFormat(showSeconds ? Locale.LongFormat
                                                   : Locale.ShortFormat);
    }

    function displayText() {
        const time = Qt.formatTime(currentTime, timeFormat());
        if (!showDate)
            return time;
        const date = Qt.formatDate(currentTime,
                                   Qt.locale().dateFormat(Locale.ShortFormat));
        return vertical ? date + "\n" + time : date + "  " + time;
    }

    Text {
        id: label
        objectName: "clockAppletLabel"
        anchors.centerIn: parent
        width: root.vertical ? parent.width - 4 : implicitWidth
        text: root.displayText()
        // AGENT-NOTE: the Luna clock is plain white Tahoma at normal weight,
        // like the Luna notification-area clock (ADR-0124, "Luna taskbar
        // rendering"). An outline or bold face at 11 px fills the counters
        // and left the digits illegible on the tray well.
        color: root.luna ? "white" : (root.colors.text ?? "white")
        font.pixelSize: root.vertical ? 9 : 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.NoWrap
    }

    // Only the Luna path names a family; every other host keeps the
    // inherited default font untouched.
    Binding {
        target: label
        property: "font.family"
        value: "Tahoma"
        when: root.luna
    }

    Timer {
        interval: root.showSeconds ? 250 : 1000
        repeat: true
        running: root.visible
        triggeredOnStart: true
        onTriggered: root.currentTime = new Date()
    }
}
