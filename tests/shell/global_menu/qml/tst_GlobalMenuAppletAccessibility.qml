// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/shell/global_menu/applet/qml" as GlobalMenuComponents

// Accessibility-path suite: activation through the REAL attached accessible
// signal, focusability truth, and provider-owned checked state under
// interactive activation.
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
            width: 360
            height: 28
            access: fakeAccess
            theme: testRoot.theme
        }
    }

    TestCase {
        name: "GlobalMenuAppletAccessibility"
        when: windowShown

        function init() {
            fakeAccess.available = false;
            fakeAccess.items = [];
            fakeAccess.activateCalls = 0;
            fakeAccess.lastActivatedId = "";
        }

        function realisticMenuItems() {
            return [
                { "id": "fileMenu", "kind": "submenu", "text": "File",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false },
                { "id": "editMenu", "kind": "submenu", "text": "Edit",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false },
                { "id": "aboutAction", "kind": "action", "text": "About",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false }
            ];
        }

        function collectEntries(node, out) {
            if (node === null || node === undefined) {
                return;
            }
            if (node.visible === false) {
                return;
            }
            if (node.objectName === "globalMenuTopLevelItem") {
                out.push(node);
            }
            const kids = node.children || [];
            for (let i = 0; i < kids.length; ++i) {
                collectEntries(kids[i], out);
            }
        }

        function test_accessibleFocusabilityTruth() {
            fakeAccess.available = true;
            fakeAccess.items = realisticMenuItems();
            fakeAccess.items.push(
                { "id": "quitAction", "kind": "action", "text": "Quit",
                  "mnemonicIndex": 0, "enabled": false, "checkable": false, "checked": false });
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 4);

            // Submenu: disabled in G0, Accessible.focusable is false
            verify(!entries[0].enabled);
            verify(!entries[0].Accessible.focusable);

            // Submenu: disabled in G0, Accessible.focusable is false
            verify(!entries[1].enabled);
            verify(!entries[1].Accessible.focusable);

            // Enabled action: enabled, Accessible.focusable is true
            verify(entries[2].enabled);
            verify(entries[2].Accessible.focusable);

            // Disabled action: disabled, Accessible.focusable is false
            verify(!entries[3].enabled);
            verify(!entries[3].Accessible.focusable);
        }

        function test_accessiblePressActivationPath() {
            // Drive the REAL attached accessible signal: emitting
            // Accessible.pressAction() invokes the Accessible.onPressAction
            // handler exactly as assistive technology does, so a missing or
            // broken connection fails this test.
            fakeAccess.available = true;
            fakeAccess.items = realisticMenuItems();
            fakeAccess.items.push(
                { "id": "quitAction", "kind": "action", "text": "Quit",
                  "mnemonicIndex": 0, "enabled": false, "checkable": false, "checked": false });
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 4);

            entries[2].Accessible.pressAction(); // enabled action: About
            compare(fakeAccess.activateCalls, 1);
            compare(fakeAccess.lastActivatedId, "aboutAction");

            entries[0].Accessible.pressAction(); // submenu: presented, non-activating
            compare(fakeAccess.activateCalls, 1);

            entries[3].Accessible.pressAction(); // disabled action
            compare(fakeAccess.activateCalls, 1);
        }

        function test_checkableEntriesActivateWithoutLocalToggleInversion() {
            // Presentation never owns toggle state: the delegate button is
            // non-toggleable, so interactive activation (Space here) requests
            // the action through the one shared path while the bound,
            // provider-owned `checked` value never locally inverts.
            fakeAccess.available = true;
            fakeAccess.items = [
                { "id": "wordWrapAction", "kind": "action", "text": "Word Wrap",
                  "mnemonicIndex": 0, "enabled": true, "checkable": true, "checked": true },
                { "id": "lineNumbersAction", "kind": "action", "text": "Line Numbers",
                  "mnemonicIndex": 0, "enabled": true, "checkable": true, "checked": false },
                { "id": "plainAction", "kind": "action", "text": "Plain",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false }
            ];
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 3);

            // The button itself is not a toggle; the accessible state carries
            // the provider-owned checked truth.
            verify(!entries[0].checkable);
            verify(!entries[1].checkable);
            verify(!entries[2].checkable);
            verify(entries[0].Accessible.checkable);
            verify(entries[0].Accessible.checked);
            verify(entries[1].Accessible.checkable);
            verify(!entries[1].Accessible.checked);
            verify(!entries[2].Accessible.checkable);

            // Initially checked: Space activates without flipping the state.
            entries[0].forceActiveFocus(Qt.TabFocusReason);
            verify(entries[0].activeFocus);
            keyClick(Qt.Key_Space);
            compare(fakeAccess.activateCalls, 1);
            compare(fakeAccess.lastActivatedId, "wordWrapAction");
            verify(entries[0].checked);
            verify(entries[0].Accessible.checked);

            // Initially unchecked: same activation, still no local toggle.
            entries[1].forceActiveFocus(Qt.TabFocusReason);
            verify(entries[1].activeFocus);
            keyClick(Qt.Key_Space);
            compare(fakeAccess.activateCalls, 2);
            compare(fakeAccess.lastActivatedId, "lineNumbersAction");
            verify(!entries[1].checked);
            verify(!entries[1].Accessible.checked);
        }
    }
}
