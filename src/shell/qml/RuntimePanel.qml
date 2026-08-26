// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Window {
    id: root

    required property var panel
    required property var theme
    required property string surfaceId

    visible: false
    color: "transparent"
    title: qsTr("QindaQt panel %1").arg(surfaceId)
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus

    PanelContent {
        anchors.fill: parent
        panel: root.panel
        theme: root.theme
        liveApplets: true
    }
}
