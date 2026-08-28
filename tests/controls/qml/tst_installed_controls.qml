// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaQt.Controls 1.0 as C

Item {
    width: 64
    height: 64

    Component {
        id: buttonFactory
        C.Button {
            text: "Apply"
            available: true
            busy: true
            error: true
            accessibleDescription: "Apply staged settings"
        }
    }
    Component {
        id: formFactory
        C.FormRow {
            label: "Profile name"
            description: "Choose a unique name"
            required: true
            errorMessage: "Already in use"
            editor: installedEditor
            C.TextField {
                id: installedEditor
                accessibleName: "Profile name"
                error: true
            }
        }
    }
    Component {
        id: stateFactory
        C.StateCard {
            status: C.StateCard.Warning
            title: "Service unavailable"
            message: "Retry after reconnecting"
            actionText: "Retry"
        }
    }
    Component {
        id: themeFactory
        C.ThemeCard {
            themeName: "Qinda Dark"
            description: "Dark semantic preview"
            checked: true
            available: true
        }
    }

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
