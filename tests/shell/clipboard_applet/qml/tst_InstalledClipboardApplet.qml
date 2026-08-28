// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaQt.Shell.ClipboardApplet 1.0

// Installed-module probe: this file deliberately does NOT reference any
// build-tree path. It imports the staged, relocatable QML module resolved
// only through the -import root of the staged prefix.
Item {
    id: testRoot
    width: 380
    height: 480

    ClipboardApplet {
        id: applet
        width: 380
        height: 480
        controller: null
    }

    TestCase {
        name: "InstalledClipboardAppletImport"
        when: windowShown

        function test_module_imports_and_instantiates() {
            verify(applet !== null, "staged module did not instantiate ClipboardApplet")
            compare(applet.objectName, "clipboardApplet")
            verify(applet.implicitWidth > 0)
        }

        function test_unconnected_controller_renders_closed() {
            verify(applet.controller === null)
            verify(findChild(applet, "clipboardEntriesList") === null
                   || !findChild(applet, "clipboardEntriesList").visible)
        }
    }
}
