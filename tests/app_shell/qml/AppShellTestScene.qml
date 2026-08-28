// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.AppShell 1.0
import QindaQt.Controls 1.0 as Qinda

ApplicationShell {
    id: window
    objectName: "applicationShellWindow"
    visible: true
    width: 640
    height: 480
    coordinator: ApplicationCoordinator {
        id: shellCoordinator
        objectName: "applicationShellCoordinator"
        applicationName: qsTr("AppShell test application")
        windowTitle: qsTr("AppShell test window")
        initialFocusObjectName: "primaryAction"
    }
    initialFocusItem: primaryAction

    Qinda.Button {
        id: primaryAction
        objectName: "primaryAction"
        anchors.centerIn: parent
        text: qsTr("Primary action")
        accessibleDescription: qsTr("Exercises the initial keyboard focus contract")
    }
}
