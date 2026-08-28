// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaQt.AppShell 1.0

TestCase {
    name: "InstalledAppShellImport"
    when: windowShown

    ApplicationCoordinator {
        id: coordinator
        applicationName: "Installed consumer"
        windowTitle: "Installed consumer window"
        initialFocusObjectName: "content"
    }

    function test_publicCoordinatorShape() {
        compare(coordinator.applicationName, "Installed consumer")
        compare(coordinator.windowTitle, "Installed consumer window")
        compare(coordinator.initialFocusObjectName, "content")
        compare(coordinator.menus.length, 0)
        verify(!coordinator.degraded)
        compare(coordinator.requestOpenFile("Open", ["not a mime type"]), 0)
        verify(coordinator.lastErrorMessage.length > 0)
    }
}
