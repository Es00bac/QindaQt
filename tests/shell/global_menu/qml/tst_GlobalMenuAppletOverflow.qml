// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/shell/global_menu/applet/qml" as GlobalMenuComponents

// Overflow/geometry suite for the measured geometry contract. The
// assertions are font-metric-independent on purpose: they check the
// invariant the contract promises (whatever the real font metrics are, the
// instantiated entries plus the +N indicator never exceed the assigned
// main-axis extent, and below-minimum hosts degrade to indicator-only)
// rather than hardcoded pixel counts.
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

        function numberedMenuItems(count, glyph) {
            const many = [];
            for (let i = 0; i < count; ++i) {
                many.push({ "id": "action" + i, "kind": "action", "text": glyph + i,
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

        // The contract invariant: instantiated entries plus the indicator
        // (when shown) never exceed the assigned main-axis extent.
        function verifyHorizontalFits(applet) {
            const layout = findChild(applet, "globalMenuHorizontalLayout");
            verify(layout !== null);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            let used = layout.visible ? layout.implicitWidth : 0;
            if (indicator.visible) {
                used += 12 + indicator.width;
            }
            verify(used <= applet.width + 0.5);
        }

        function verifyVerticalFits(applet) {
            const layout = findChild(applet, "globalMenuVerticalLayout");
            verify(layout !== null);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            let used = layout.visible ? layout.implicitHeight : 0;
            if (indicator.visible) {
                used += 4 + indicator.height;
            }
            verify(used <= applet.height + 0.5);
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
            // Stacked vertically: each entry starts below the previous one,
            // on the deterministic 24 px entry height.
            compare(entries[0].height, 24);
            verify(entries[1].y >= entries[0].y + entries[0].height);
            verify(entries[2].y >= entries[1].y + entries[1].height);
            verifyVerticalFits(applet);
        }

        function test_wideHostCollapsesOnlyAtTheCountCap() {
            // A host wide enough for everything still applies the count cap:
            // 12 items, cap 8 → 8 entries and a +4 affordance.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "Item ");
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
            verifyHorizontalFits(applet);
        }

        function test_narrowHorizontalHostCollapsesByAssignedWidth() {
            // The collapse reacts to the assigned width; whatever the real
            // font metrics are, the retained row plus the indicator fit the
            // assigned extent and the remainder is counted honestly.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "Item ");
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "width": 200 });
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            const entries = [];
            collectEntries(applet, entries);
            verify(entries.length >= 0);
            verify(entries.length < 12);
            if (indicator.visible) {
                compare(indicator.text, "+" + (12 - entries.length));
                compare(indicator.Accessible.name,
                        (12 - entries.length) + " more menu entries");
            }
            verifyHorizontalFits(applet);
        }

        function test_wideGlyphsStillFitAssignedWidth() {
            // Wide glyphs are exactly the case the old string-length
            // heuristic got wrong: with a bounding-measurement contract the
            // instantiated row can still never exceed the assigned width.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "WWWWWWWWWW");
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "width": 360 });
            const entries = [];
            collectEntries(applet, entries);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            if (entries.length < 12) {
                verify(indicator.visible);
            }
            verifyHorizontalFits(applet);
        }

        function test_belowMinimumHorizontalHostDegradesToIndicatorOnly() {
            // A host too narrow for even one measured entry shows no partial
            // labels; it degrades to the (hidden, if it cannot fit)
            // indicator-only state instead of clipping a real label.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "Item ");
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "width": 8 });
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 0);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(!indicator.visible);
            verifyHorizontalFits(applet);
        }

        function test_narrowVerticalHostKeepsIndicatorInsideGeometry() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(3, "Item ");
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "vertical": true, "height": 48 });
            const layout = findChild(applet, "globalMenuVerticalLayout");
            verify(layout !== null);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(indicator !== null);
            const entries = [];
            collectEntries(layout, entries);
            verify(entries.length < 3);
            if (indicator.visible) {
                compare(indicator.text, "+" + (3 - entries.length));
            }
            verifyVerticalFits(applet);
        }

        function test_belowMinimumVerticalHostDegradesToIndicatorOnly() {
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(3, "Item ");
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "vertical": true, "height": 12 });
            const entries = [];
            collectEntries(applet, entries);
            compare(entries.length, 0);
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(!indicator.visible);
            verifyVerticalFits(applet);
        }

        function test_negativeEntryLimitIsClamped() {
            // A negative count must not reach slice()'s negative-index
            // semantics ("drop from the end"): it clamps to one visible
            // entry, with the rest honestly counted as overflow.
            fakeAccess.available = true;
            fakeAccess.items = numberedMenuItems(12, "Item ");
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
            verifyHorizontalFits(applet);
        }
    }
}
