// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaQt.SettingsApp.Appearance

Item {
    id: sceneRoot

    required property var stubModel
    signal closeRequested()

    width: 640
    height: 480

    AppearancePage {
        anchors.fill: parent
        appearanceSettings: sceneRoot.stubModel
        onCloseRequested: sceneRoot.closeRequested()
    }
}
