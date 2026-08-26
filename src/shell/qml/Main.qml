// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QindaQt.Shell

ApplicationWindow {
    id: root

    width: requestedPreviewWidth
    height: requestedPreviewHeight
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: qsTr("QindaQt Shell Preview")
    color: themeCatalog.current.colors?.canvas ?? "#171a18"

    font.family: themeCatalog.current.fontFamily ?? "sans-serif"

    DesktopPreview {
        anchors.fill: parent
        profile: profileCatalog.current
        theme: themeCatalog.current
    }

    PreviewControls {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 42
        z: 100
    }
}
