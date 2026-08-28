// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/shell/global_menu/applet/qml" as GlobalMenuComponents

// AGENT-NOTE: This equality-boundary case is isolated from the broader
// overflow matrix so both suites remain below the source-shape decomposition
// threshold. Keep the shared geometry contract aligned with the sibling suite.
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

        function activate(actionId) {
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
        name: "GlobalMenuAppletVerticalBoundary"
        when: windowShown

        function init() {
            fakeAccess.available = false;
            fakeAccess.items = [];
        }

        function createItem(id, text) {
            return {
                "id": id,
                "kind": "action",
                "text": text,
                "mnemonicIndex": -1,
                "enabled": true,
                "checkable": false,
                "checked": false
            };
        }

        function createApplet(config) {
            return createTemporaryObject(appletComponent, testRoot, config);
        }

        function collectEntries(node, out) {
            if (!node || node.visible === false)
                return;

            if (node.objectName === "globalMenuTopLevelItem")
                out.push(node);

            const children = node.children || [];
            for (let i = 0; i < children.length; ++i)
                collectEntries(children[i], out);
        }

        function verifyFits(applet) {
            const layout = findChild(applet, "globalMenuVerticalLayout");
            const indicator = findChild(applet, "globalMenuOverflowIndicator");
            verify(layout !== null);
            let used = layout.visible ? layout.implicitHeight : 0;
            if (indicator.visible) {
                used += 4 + indicator.height;
                verify(indicator.y + indicator.height <= applet.height + 0.5);
            }
            verify(used <= applet.height + 0.5);
        }

        function test_calculatedEqualityBoundaryFitsExactBudget() {
            fakeAccess.available = true;
            fakeAccess.items = [createItem("a0", "File"), createItem("a1", "Edit")];
            const probe = createApplet({"vertical": true, "height": 1000});
            const indicatorHeight = probe.measuredIndicatorHeight();
            const exactHeight = 24 + 4 + indicatorHeight;

            const exactApplet = createApplet({"vertical": true, "height": exactHeight});
            const exactEntries = [];
            collectEntries(exactApplet, exactEntries);
            compare(exactEntries.length, 1);
            const exactIndicator = findChild(exactApplet, "globalMenuOverflowIndicator");
            verify(exactIndicator && exactIndicator.visible);
            verifyFits(exactApplet);
            const exactColumn = findChild(exactApplet, "globalMenuVerticalLayout");
            verify(exactColumn.implicitHeight + 4 + exactIndicator.height
                   <= exactApplet.height + 0.5);

            const shortApplet = createApplet({"vertical": true, "height": exactHeight - 1});
            const shortEntries = [];
            collectEntries(shortApplet, shortEntries);
            compare(shortEntries.length, 0);
            verifyFits(shortApplet);

            const indicatorApplet = createApplet({
                "vertical": true,
                "height": indicatorHeight + 4
            });
            const indicator = findChild(indicatorApplet, "globalMenuOverflowIndicator");
            verify(indicator && indicator.visible);
            verifyFits(indicatorApplet);

            const hiddenApplet = createApplet({
                "vertical": true,
                "height": indicatorHeight + 3
            });
            const hiddenIndicator = findChild(hiddenApplet, "globalMenuOverflowIndicator");
            verify(hiddenIndicator && !hiddenIndicator.visible);
            verifyFits(hiddenApplet);
        }
    }
}
