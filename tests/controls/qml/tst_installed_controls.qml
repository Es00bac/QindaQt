// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaQt.Controls 1.0 as C

Item {
    width: 64
    height: 64

    Component { id: buttonFactory; C.Button {} }
    Component { id: formFactory; C.FormRow { editor: editor; C.TextField { id: editor } } }
    Component { id: stateFactory; C.StateCard {} }
    Component { id: themeFactory; C.ThemeCard {} }

    TestCase {
        name: "InstalledControlsModule"
        when: windowShown

        function test_publicTypesResolveFromInstalledModule() {
            compare(buttonFactory.status, Component.Ready)
            compare(formFactory.status, Component.Ready)
            compare(stateFactory.status, Component.Ready)
            compare(themeFactory.status, Component.Ready)
        }
    }
}
