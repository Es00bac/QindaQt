// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest

Item {
    width: 32
    height: 32

    Loader {
        id: bridge
        source: "QindaQtAppearanceBridge.qml"
    }

    TestCase {
        name: "OptionalAppearanceBridge"
        when: windowShown

        function test_missingModuleLeavesHostAlive() {
            tryCompare(bridge, "status", Loader.Error)
            verify(bridge.item === null)
        }
    }
}
