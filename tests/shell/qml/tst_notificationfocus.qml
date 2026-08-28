// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQml.Models
import QtTest
import "../../../src/shell/qml" as ShellComponents

Item {
    id: testRoot
    width: 900
    height: 700

    ListModel {
        id: activeItems
        ListElement {
            notificationId: 7
            applicationName: "QindaQt Test"
            summary: "Plain summary"
            body: "Plain body"
            urgency: 1
            actions: [
                ListElement { key: "one"; label: "First" },
                ListElement { key: "two"; label: "Second" },
                ListElement { key: "three"; label: "Third" }
            ]
            active: true
        }
    }
    ListModel {
        id: historyItems
        ListElement {
            notificationId: 9
            applicationName: "QindaQt Test"
            summary: "History"
            body: "Retained history"
            urgency: 1
            actions: []
            active: false
        }
    }
    ListModel { id: popupItems }

    QtObject {
        id: fakePresentation
        property var activeModel: activeItems
        property var popupModel: popupItems
        property var historyModel: historyItems
        property bool centerOpen: true
        property bool doNotDisturbEnabled: false
        property bool operationBusy: false
        property string operationErrorText: ""
        function setCenterOpen(open) { centerOpen = open; }
        function closePopup(notificationId) {}
        function clearHistory() { historyItems.clear(); }
        function dismiss(notificationId) { return true; }
        function invokeAction(notificationId, actionKey) { return true; }
    }
    QtObject {
        id: fakeQuieting
        property bool enabled: false
        property bool canToggle: true
        property bool conflict: false
        property bool unavailable: false
        property string statusText: ""
        property string errorText: ""
        function requestSet(value) { enabled = value; return true; }
        function retry() {}
        function applyMyChoice() { return true; }
    }
    QtObject {
        id: fakeSettingsLauncher
        function openNotifications() { return true; }
    }
    property var theme: ({
        "cornerRadius": 10,
        "colors": {
            "surface": "#222624", "surfaceRaised": "#2c312e",
            "border": "#3c433f", "text": "#f2f1eb",
            "textMuted": "#a9afa9", "accent": "#8fc8b7",
            "accentText": "#10201b", "danger": "#f07c76"
        }
    })

    Component {
        id: centerComponent
        ShellComponents.NotificationCenter {
            presentation: fakePresentation
            quietingSettings: fakeQuieting
            settingsLauncher: fakeSettingsLauncher
            theme: testRoot.theme
        }
    }

    TestCase {
        name: "NotificationFocus"
        when: windowShown

        function test_naturalChainIncludesEnabledCardsAndBothDirections() {
            const center = createTemporaryObject(centerComponent, testRoot,
                                                 { "width": 440,
                                                   "height": 640 });
            verify(center !== null);
            center.visible = true;
            const closeButton = findChild(center,
                                          "notificationCenterCloseButton");
            verify(closeButton !== null);

            function traversalItems(forward) {
                const items = [];
                let current = closeButton;
                for (let index = 0; index < 16; ++index) {
                    current = current.nextItemInFocusChain(forward);
                    if (current === null)
                        return [];
                    if (current === closeButton)
                        break;
                    if (current.objectName.length > 0)
                        items.push(current);
                }
                return items;
            }

            function traversal(forward) {
                return traversalItems(forward).map(function(item) {
                    return item.objectName;
                });
            }

            function namedItem(items, objectName) {
                for (let index = 0; index < items.length; ++index) {
                    if (items[index].objectName === objectName)
                        return items[index];
                }
                return null;
            }

            tryVerify(function() {
                const names = traversal(true);
                return names.includes("notificationPrimaryAction")
                    && names.includes("notificationMoreActions")
                    && names.includes("notificationDismiss")
                    && names.includes("notificationSettingsRouteButton")
                    && names.includes("notificationDoNotDisturbButton")
                    && names.includes("notificationClearHistoryButton");
            });
            const forward = traversal(true);
            const reverse = traversal(false);
            compare(reverse.slice().reverse(), forward);

            // Repeater delegates participate in the production focus chain but
            // are not stable QObject children for TestCase.findChild(). Keep
            // accessibility assertions bound to the same traversed instances.
            const items = traversalItems(true);
            const primary = namedItem(items, "notificationPrimaryAction");
            const more = namedItem(items, "notificationMoreActions");
            const dismiss = namedItem(items, "notificationDismiss");
            const settings = namedItem(items,
                                       "notificationSettingsRouteButton");
            const dnd = namedItem(items, "notificationDoNotDisturbButton");
            const clear = namedItem(items,
                                    "notificationClearHistoryButton");
            verify(primary !== null, "primary action was not instantiated");
            verify(more !== null, "overflow action was not instantiated");
            verify(dismiss !== null, "dismiss action was not instantiated");
            verify(settings !== null, "settings action was not instantiated");
            verify(dnd !== null, "DND action was not instantiated");
            verify(clear !== null, "clear-history action was not instantiated");
            compare(primary.Accessible.role, Accessible.Button);
            compare(primary.Accessible.name, "First");
            compare(more.Accessible.role, Accessible.Button);
            compare(more.Accessible.name, "More notification actions");
            compare(dismiss.Accessible.role, Accessible.Button);
            compare(dismiss.Accessible.name, "Dismiss notification");
            compare(settings.Accessible.role, Accessible.Button);
            compare(settings.Accessible.name, "Notification settings");
            compare(dnd.Accessible.role, Accessible.CheckBox);
            compare(dnd.Accessible.name, "Turn on Do Not Disturb");
            compare(clear.Accessible.role, Accessible.Button);
            compare(closeButton.Accessible.role, Accessible.Button);
            compare(closeButton.Accessible.name, "Close notification center");
        }
    }
}
