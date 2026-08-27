// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    required property var quietingSettings
    required property string route
    visible: true
    width: 560
    height: 420
    minimumWidth: 420
    minimumHeight: 320
    title: qsTr("QindaQt Settings — Notifications")

    NotificationsPage {
        anchors.fill: parent
        quietingSettings: root.quietingSettings
        onCloseRequested: root.close()
    }
}
