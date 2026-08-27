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
        anchors.centerIn: parent
        width: root.vertical ? parent.width - 4 : implicitWidth
        text: root.displayText()
        color: root.colors.text ?? "white"
        font.pixelSize: root.vertical ? 9 : 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.NoWrap
    }

    Timer {
        interval: root.showSeconds ? 250 : 1000
        repeat: true
        running: root.visible
        triggeredOnStart: true
        onTriggered: root.currentTime = new Date()
    }
}
