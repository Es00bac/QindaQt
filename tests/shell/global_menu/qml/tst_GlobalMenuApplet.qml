// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/shell/global_menu/applet/qml" as GlobalMenuComponents

Item {
    id: testRoot
    width: 360
    height: 60

    property var theme: ({
        "colors": {
            "text": "#f2f1eb",
            "textMuted": "#a9afa9"
        }
    })

    QtObject {
        id: fakeAccess
        property bool available: false
        property var items: []
        property int activateCalls: 0
        property string lastActivatedId: ""
        function activate(actionId) {
            ++activateCalls;
            lastActivatedId = actionId;
        }
    }

    Component {
        id: appletComponent
        GlobalMenuComponents.GlobalMenuApplet {
            access: fakeAccess
            theme: testRoot.theme
        }
    }

    TestCase {
        name: "GlobalMenuApplet"
        when: windowShown

        function init() {
            fakeAccess.available = false;
            fakeAccess.items = [];
            fakeAccess.activateCalls = 0;
            fakeAccess.lastActivatedId = "";
        }

        function test_nullAccessShowsUnavailable() {
            const applet = createTemporaryObject(appletComponent, testRoot, { "access": null });
            verify(applet !== null);
            compare(applet.Accessible.name, "Menu unavailable");
        }

        function test_unavailableAccessShowsUnavailable() {
            const applet = createTemporaryObject(appletComponent, testRoot);
            verify(applet !== null);
            compare(applet.Accessible.name, "Menu unavailable");
        }

        function test_availableAccessRendersTopLevelItems() {
            fakeAccess.available = true;
            fakeAccess.items = [
                { "id": "fileMenu", "text": "File", "mnemonicIndex": 0, "enabled": true,
                  "checkable": false, "checked": false }
            ];
            const applet = createTemporaryObject(appletComponent, testRoot);
            verify(applet !== null);
            compare(applet.Accessible.name, "Application menu");
            const entry = findChild(applet, "globalMenuTopLevelItem");
            verify(entry !== null);
            compare(entry.Accessible.name, "File");
        }

        function test_clickingEnabledItemActivatesById() {
            fakeAccess.available = true;
            fakeAccess.items = [
                { "id": "fileNewAction", "text": "New", "mnemonicIndex": 0, "enabled": true,
                  "checkable": false, "checked": false }
            ];
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entry = findChild(applet, "globalMenuTopLevelItem");
            verify(entry !== null);
            mouseClick(entry);
            compare(fakeAccess.activateCalls, 1);
            compare(fakeAccess.lastActivatedId, "fileNewAction");
        }

        function test_disabledItemIsNotClickable() {
            fakeAccess.available = true;
            fakeAccess.items = [
                { "id": "fileQuitAction", "text": "Quit", "mnemonicIndex": 0, "enabled": false,
                  "checkable": false, "checked": false }
            ];
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entry = findChild(applet, "globalMenuTopLevelItem");
            verify(entry !== null);
            verify(!entry.enabled);
            mouseClick(entry);
            compare(fakeAccess.activateCalls, 0);
        }
    }
}
