// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/shell/global_menu/applet/qml" as GlobalMenuComponents

// Overflow/geometry suite: bounded collapse must react to the assigned
// main-axis extent (width horizontal, height vertical), keep the +N
// affordance inside the clipped geometry, and clamp hostile count limits.
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
        name: "GlobalMenuAppletOverflow"
        when: windowShown

        function init() {
            fakeAccess.available = false;
            fakeAccess.items = [];
            fakeAccess.activateCalls = 0;
            fakeAccess.lastActivatedId = "";
        }

        function numberedMenuItems(count) {
            const many = [];
            for (let i = 0; i < count; ++i) {
                many.push({ "id": "action" + i, "kind": "action", "text": "Item " + i,
                            "mnemonicIndex": -1, "enabled": true, "checkable": false,
                            "checked": false });
            }
            return many;
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

        function test_verticalLayoutStacksEntries() {
            fakeAccess.available = true;
            fakeAccess.items = [
                { "id": "fileMenu", "kind": "submenu", "text": "File",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false },
                { "id": "editMenu", "kind": "submenu", "text": "Edit",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false },
                { "id": "aboutAction", "kind": "action", "text": "About",
                  "mnemonicIndex": 0, "enabled": true, "checkable": false, "checked": false }
            ];
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "vertical": true, "height": 120 });
            verify(applet !== null);
            const layout = findChild(applet, "globalMenuVerticalLayout");
            verify(layout !== null);
            verify(layout.visible);
            const entries = [];
            collectEntries(layout, entries);
            compare(entries.length, 3);
            // Stacked vertically: each entry starts below the previous one.
            verify(entries[1].y >= entries[0].y + entries[0].height);
            verify(entries[2].y >= entries[1].y + entries[1].height);
        }

        function test_wideHostCollapsesOnlyAtTheCountCap() {
            // A host wide enough for everything still applies the count cap:
            // 12 items, cap 8 → 8 entries and a +4 affordance.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12);
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "width": 2000 });
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            verify(indicator.visible);
            compare(indicator.text, "+4");
            compare(indicator.Accessible.name, "4 more menu entries");
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 8);
            verify(applet.clip);
        }

        function test_narrowHorizontalHostCollapsesByAssignedWidth() {
            // 200 px wide host minus the reserved +N affordance leaves room
            // for exactly two estimated entries; the rest collapses.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12);
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "width": 200 });
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            verify(indicator.visible);
            compare(indicator.text, "+10");
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 2);
            // The affordance stays inside the assigned (clipped) geometry.
            verify(indicator.x + indicator.width <= applet.width + 0.5);
        }

        function test_narrowVerticalHostKeepsIndicatorInsideGeometry() {
            // A 48 px tall host fits exactly one 28 px entry slot plus the
            // reserved +N affordance; the indicator is part of implicit
            // height and stays inside the assigned geometry.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(3);
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "vertical": true, "height": 48 });
            const layout = findChild(applet, "globalMenuVerticalLayout");
            verify(layout !== null);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            verify(indicator.visible);
            compare(indicator.text, "+2");
            const entries = [];
            collectEntries(layout, entries);
            compare(entries.length, 1);
            verify(applet.implicitHeight >= indicator.y + indicator.height);
            verify(indicator.y + indicator.height <= applet.height + 0.5);
        }

        function test_negativeEntryLimitIsClamped() {
            // A negative count must not reach slice()'s negative-index
            // semantics ("drop from the end"): it clamps to one visible
            // entry, with the rest honestly counted as overflow.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12);
            const applet = createTemporaryObject(
                               appletComponent, testRoot,
                               { "width": 2000, "maximumVisibleEntries": -5 });
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 1);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            verify(indicator.visible);
            compare(indicator.text, "+11");
        }
    }
}
