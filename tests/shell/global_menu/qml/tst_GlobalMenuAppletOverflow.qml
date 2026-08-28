// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/shell/global_menu/applet/qml" as GlobalMenuComponents

// Overflow/geometry suite: instantiated entries plus indicator never
// exceed assigned extent, and below-minimum hosts degrade cleanly.
Item {
    id: testRoot

    property var theme: ({
        "colors": {
            "text": "#f2f1eb",
            "textMuted": "#a9afa9"
        }
    })

    width: 360
    height: 60

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
        function init() {
            fakeAccess.available = false;
            fakeAccess.items = [];
            fakeAccess.activateCalls = 0;
            fakeAccess.lastActivatedId = "";
        }

        function createItem(id, text, kind = "action") {
            return {
                "id": id,
                "kind": kind,
                "text": text,
                "mnemonicIndex": -1,
                "enabled": true,
                "checkable": false,
                "checked": false
            };
        }

        function numberedMenuItems(count, glyph) {
            const list = [];
            for (let i = 0; i < count; ++i) {
                list.push(createItem("action" + i, glyph + i));
            }
            return list;
        }

        function createApplet(config) {
            return createTemporaryObject(appletComponent, testRoot, config || {
            });
        }

        function collectEntries(node, out) {
            if (!node || node.visible === false)
                return ;

            if (node.objectName === "globalMenuTopLevelItem")
                out.push(node);

            const kids = node.children || [];
            for (let i = 0; i < kids.length; ++i) {
                collectEntries(kids[i], out);
            }
        }

        function verifyFits(applet) {
            const hLayout = findChild(applet, "globalMenuHorizontalLayout");
            const vLayout = findChild(applet, "globalMenuVerticalLayout");
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            if (applet.vertical) {
                verify(vLayout !== null);
                let used = vLayout.visible ? vLayout.implicitHeight : 0;
                if (indicator.visible) {
                    used += 4 + indicator.height;
                    verify(indicator.y + indicator.height <= applet.height + 0.5);
                }
                verify(used <= applet.height + 0.5);
            } else {
                verify(hLayout !== null);
                let used = hLayout.visible ? hLayout.implicitWidth : 0;
                if (indicator.visible) {
                    used += applet.spacing + indicator.width;
                    verify(indicator.x + indicator.width <= applet.width + 0.5);
                }
                verify(used <= applet.width + 0.5);
            }
        }

        function test_verticalLayoutStacksEntries() {
            fakeAccess.available = true;
            fakeAccess.items = [createItem("f", "File", "submenu"), createItem("e", "Edit", "submenu"), createItem("a", "About")];
            const applet = createApplet({
                "vertical": true,
                "height": 120
            });
            verify(applet !== null);
            const layout = findChild(applet, "globalMenuVerticalLayout");
            verify(layout && layout.visible);
            const entries = [];
            collectEntries(layout, entries);
            compare(entries.length, 3);
            compare(entries[0].height, 24);
            verify(entries[1].y >= entries[0].y + entries[0].height);
            verify(entries[2].y >= entries[1].y + entries[1].height);
            verifyFits(applet);
        }

        function test_wideHostCollapsesOnlyAtTheCountCap() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "Item ");
            const applet = createApplet({
                "width": 2000
            });
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator && indicator.visible);
            compare(indicator.text, "+4");
            compare(indicator.Accessible.name, "4 more menu entries");
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 8);
            verify(applet.clip);
            verifyFits(applet);
        }

        function test_narrowHorizontalHostCollapsesByAssignedWidth() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "Item ");
            const applet = createApplet({
                "width": 200
            });
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            const entries = [];
            collectEntries(applet, entries);
            verify(entries.length >= 0 && entries.length < 12);
            if (indicator.visible) {
                compare(indicator.text, "+" + (12 - entries.length));
                compare(indicator.Accessible.name, (12 - entries.length) + " more menu entries");
            }
            verifyFits(applet);
        }

        function test_wideGlyphsStillFitAssignedWidth() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "WWWWWWWWWW");
            const applet = createApplet({
                "width": 360
            });
            const entries = [];
            collectEntries(applet, entries);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            if (entries.length < 12)
                verify(indicator.visible);

            verifyFits(applet);
        }

        function test_belowMinimumHorizontalHostDegradesToIndicatorOnly() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "Item ");
            const applet = createApplet({
                "width": 8
            });
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 0);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(!indicator.visible);
            verifyFits(applet);
        }

        function test_narrowVerticalHostKeepsIndicatorInsideGeometry() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(3, "Item ");
            const applet = createApplet({
                "vertical": true,
                "height": 48
            });
            const layout = findChild(applet, "globalMenuVerticalLayout");
            verify(layout !== null);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            const entries = [];
            collectEntries(layout, entries);
            verify(entries.length < 3);
            if (indicator.visible)
                compare(indicator.text, "+" + (3 - entries.length));

            verifyFits(applet);
        }

        function test_belowMinimumVerticalHostDegradesToIndicatorOnly() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(3, "Item ");
            const applet = createApplet({
                "vertical": true,
                "height": 12
            });
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 0);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(!indicator.visible);
            verifyFits(applet);
        }

        function test_negativeEntryLimitIsClamped() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "Item ");
            const applet = createApplet({
                "width": 2000,
                "maximumVisibleEntries": -5
            });
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 1);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator && indicator.visible);
            compare(indicator.text, "+11");
            verifyFits(applet);
        }

        function test_horizontalCalculatedEqualityBoundaryFitsExactBudget() {
            fakeAccess.available = true;
            fakeAccess.items = [createItem("a0", "File"), createItem("a1", "Edit")];
            const probe = createApplet({
                "width": 1000
            });
            const exactWidth = probe.measuredEntryWidth(fakeAccess.items[0]) + probe.spacing + probe.measuredIndicatorWidth();
            const appletExact = createApplet({
                "width": exactWidth
            });
            const entriesExact = [];
            collectEntries(appletExact, entriesExact);
            compare(entriesExact.length, 1);
            const indExact = findChild(appletExact, "globalMenuOverflowIndicator");
            verify(indExact && indExact.visible);
            verifyFits(appletExact);
            const rowExact = findChild(appletExact, "globalMenuHorizontalLayout");
            verify(rowExact.implicitWidth + probe.spacing + indExact.width <= appletExact.width + 0.5);
            const appletMinusOne = createApplet({
                "width": exactWidth - 1
            });
            const entriesMinusOne = [];
            collectEntries(appletMinusOne, entriesMinusOne);
            compare(entriesMinusOne.length, 0);
            verifyFits(appletMinusOne);
        }

        function test_verticalCalculatedEqualityBoundaryFitsExactBudget() {
            fakeAccess.available = true;
            fakeAccess.items = [createItem("a0", "File"), createItem("a1", "Edit")];
            const probe = createApplet({
                "vertical": true,
                "height": 1000
            });
            const indH = probe.measuredIndicatorHeight();
            const exactH = 24 + 4 + indH;
            const appletExact = createApplet({
                "vertical": true,
                "height": exactH
            });
            const entriesExact = [];
            collectEntries(appletExact, entriesExact);
            compare(entriesExact.length, 1);
            const indExact = findChild(appletExact, "globalMenuOverflowIndicator");
            verify(indExact && indExact.visible);
            verifyFits(appletExact);
            const colExact = findChild(appletExact, "globalMenuVerticalLayout");
            verify(colExact.implicitHeight + 4 + indExact.height <= appletExact.height + 0.5);
            verify(indExact.y + indExact.height <= appletExact.height + 0.5);
            const appletMinusOne = createApplet({
                "vertical": true,
                "height": exactH - 1
            });
            const entriesMinusOne = [];
            collectEntries(appletMinusOne, entriesMinusOne);
            compare(entriesMinusOne.length, 0);
            verifyFits(appletMinusOne);
            const appletOnly = createApplet({
                "vertical": true,
                "height": indH + 4
            });
            const indOnly = findChild(appletOnly, "globalMenuOverflowIndicator");
            verify(indOnly && indOnly.visible && indOnly.y + indOnly.height <= appletOnly.height + 0.5);
            verifyFits(appletOnly);
            const appletHidden = createApplet({
                "vertical": true,
                "height": indH + 3
            });
            const indHidden = findChild(appletHidden, "globalMenuOverflowIndicator");
            verify(indHidden && !indHidden.visible);
            verifyFits(appletHidden);
        }

        name: "GlobalMenuAppletOverflow"
        when: windowShown
    }

}
