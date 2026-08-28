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

    // Mirrors the facade's documented projection shape exactly: top-level
    // entries with {id, kind, text, mnemonicIndex, enabled, checkable,
    // checked}, hidden items and separators already omitted by the facade.
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
            // Deterministic host geometry: tests override only the axis they
            // constrain. 360 wide fits three realistic entries; 28 tall fits
            // one horizontal row.
            width: 360
            height: 28
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
            // Only the active layout's delegates count; the mirrored layout
            // stays instantiated but invisible.
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

        function test_availableAccessRendersRealisticTopLevel() {
            fakeAccess.available = true;
            fakeAccess.items = realisticMenuItems();
            const applet = createTemporaryObject(appletComponent, testRoot);
            verify(applet !== null);
            compare(applet.Accessible.name, "Application menu");
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 3);
            // Realistic menus present submenus alongside actions; every
            // entry keeps an accessible name even when non-activating.
            compare(entries[0].Accessible.name, "File (submenu unavailable)");
            compare(entries[2].Accessible.name, "About");
        }

        function test_submenuEntryIsVisibleButNotActivating() {
            fakeAccess.available = true;
            fakeAccess.items = realisticMenuItems();
            const applet = createTemporaryObject(appletComponent, testRoot);
            const submenuEntry = findChild(applet, "globalMenuTopLevelItem");
            verify(submenuEntry !== null);
            // Submenus are presented honestly: visible, disabled, not a
            // clickable fake of an interaction G0 does not implement.
            verify(submenuEntry.visible);
            verify(!submenuEntry.enabled);
            compare(submenuEntry.Accessible.role, Accessible.MenuItem);
            mouseClick(submenuEntry);
            compare(fakeAccess.activateCalls, 0);
            submenuEntry.pressAction();
            compare(fakeAccess.activateCalls, 0);
        }

        function test_clickingEnabledActionActivatesById() {
            fakeAccess.available = true;
            fakeAccess.items = [
                { "id": "aboutAction", "kind": "action", "text": "About",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false }
            ];
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entry = findChild(applet, "globalMenuTopLevelItem");
            verify(entry !== null);
            verify(entry.enabled);
            mouseClick(entry);
            compare(fakeAccess.activateCalls, 1);
            compare(fakeAccess.lastActivatedId, "aboutAction");
        }

        function test_disabledActionIsNotClickable() {
            fakeAccess.available = true;
            fakeAccess.items = [
                { "id": "quitAction", "kind": "action", "text": "Quit",
                  "mnemonicIndex": 0, "enabled": false, "checkable": false, "checked": false }
            ];
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entry = findChild(applet, "globalMenuTopLevelItem");
            verify(entry !== null);
            verify(!entry.enabled);
            mouseClick(entry);
            compare(fakeAccess.activateCalls, 0);
        }

        function test_keyboardActivationOfFocusedAction() {
            fakeAccess.available = true;
            fakeAccess.items = [
                { "id": "aboutAction", "kind": "action", "text": "About",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false }
            ];
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entry = findChild(applet, "globalMenuTopLevelItem");
            verify(entry !== null);
            entry.forceActiveFocus(Qt.TabFocusReason);
            verify(entry.activeFocus);
            keyClick(Qt.Key_Space);
            compare(fakeAccess.activateCalls, 1);
            compare(fakeAccess.lastActivatedId, "aboutAction");
        }

        function test_keyboardFocusSkipsDisabledSubmenuEntries() {
            // Honest keyboard contract: disabled submenu delegates decline
            // active focus entirely (Qt Item behavior), so keyboard users
            // navigate straight to the enabled actions — focus is never dead
            // on an entry that cannot activate.
            fakeAccess.available = true;
            fakeAccess.items = realisticMenuItems();
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 3);

            // Enabled action entries take keyboard focus.
            entries[2].forceActiveFocus(Qt.TabFocusReason);
            verify(entries[2].activeFocus);
            keyClick(Qt.Key_Space);
            compare(fakeAccess.activateCalls, 1);
            compare(fakeAccess.lastActivatedId, "aboutAction");

            // Disabled submenu delegates refuse focus and never activate.
            entries[0].forceActiveFocus(Qt.TabFocusReason);
            verify(!entries[0].activeFocus);

            entries[1].forceActiveFocus(Qt.TabFocusReason);
            verify(!entries[1].activeFocus);
            compare(fakeAccess.activateCalls, 1);
        }

        function test_keyboardDoesNotActivateDisabledOrSubmenuEntries() {
            fakeAccess.available = true;
            fakeAccess.items = realisticMenuItems();
            fakeAccess.items.push(
                { "id": "quitAction", "kind": "action", "text": "Quit",
                  "mnemonicIndex": 0, "enabled": false, "checkable": false, "checked": false });
            const applet = createTemporaryObject(appletComponent, testRoot);
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 4);
            const quitEntry = entries[3];
            const submenuEntry = entries[0];
            verify(!quitEntry.enabled);
            verify(!submenuEntry.enabled);
            quitEntry.forceActiveFocus(Qt.TabFocusReason);
            verify(!quitEntry.activeFocus);
            submenuEntry.forceActiveFocus(Qt.TabFocusReason);
            verify(!submenuEntry.activeFocus);
            compare(fakeAccess.activateCalls, 0);
        }

        function test_horizontalIsDefaultLayout() {
            fakeAccess.available = true;
            fakeAccess.items = realisticMenuItems();
            const applet = createTemporaryObject(appletComponent, testRoot);
            const layout = findChild(applet, "globalMenuVerticalLayout");
            verify(layout !== null);
            verify(!layout.visible);
            // Entries sit on one baseline in the horizontal layout.
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 3);
            compare(entries[0].y, entries[1].y);
            compare(entries[1].y, entries[2].y);
        }

        function test_noOverflowHasNoIndicator() {
            fakeAccess.available = true;
            fakeAccess.items = realisticMenuItems();
            const applet = createTemporaryObject(appletComponent, testRoot);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            verify(!indicator.visible);
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 3);
        }
    }
}
