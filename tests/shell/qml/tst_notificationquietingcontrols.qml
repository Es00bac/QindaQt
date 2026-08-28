// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQml.Models
import QtTest
import "../../../src/shell/qml" as ShellComponents

Item {
    id: testRoot
    width: 900
    height: 700

    ListModel { id: activeItems }
    ListModel { id: historyItems }

    QtObject {
        id: fakePresentation
        property var activeModel: activeItems
        property var popupModel: activeItems
        property var historyModel: historyItems
        property bool centerOpen: false
        property bool doNotDisturbEnabled: false
        property bool operationBusy: false
        property string operationErrorText: ""
        function setCenterOpen(open) { centerOpen = open; }
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
        property int requests: 0
        property int retries: 0
        function requestSet(value) { ++requests; enabled = value; return true; }
        function retry() { ++retries; }
        function applyMyChoice() { return true; }
    }

    QtObject {
        id: fakeSettingsLauncher
        property int launches: 0
        function openNotifications() { ++launches; return true; }
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
        name: "NotificationQuietingControls"
        when: windowShown

        function init() {
            fakePresentation.operationBusy = false;
            fakePresentation.operationErrorText = "";
            fakeQuieting.enabled = false;
            fakeQuieting.canToggle = true;
            fakeQuieting.conflict = false;
            fakeQuieting.unavailable = false;
            fakeQuieting.statusText = "";
            fakeQuieting.errorText = "";
            fakeQuieting.requests = 0;
            fakeQuieting.retries = 0;
            fakeSettingsLauncher.launches = 0;
        }

        function test_settingsBackedControlIsAccessibleAndKeyboardOrdered() {
            const center = createTemporaryObject(centerComponent, testRoot);
            verify(center !== null);
            center.width = 384;
            center.height = 284;
            center.visible = true;
            const dnd = findChild(center, "notificationDoNotDisturbButton");
            const title = findChild(center, "notificationCenterTitle");
            const clear = findChild(center, "notificationClearHistoryButton");
            const close = findChild(center, "notificationCenterCloseButton");
            const status = findChild(center, "notificationOperationStatus");
            const settings = findChild(center, "notificationSettingsRouteButton");
            verify(dnd && title && clear && close && status && settings);
            wait(0);
            verify(title.x + title.width <= dnd.x);
            verify(dnd.x + dnd.width <= clear.x);
            verify(clear.x + clear.width <= close.x);
            // Empty history disables Clear. Natural Qt focus traversal must
            // skip disabled controls while preserving a reversible cycle.
            compare(dnd.nextItemInFocusChain(true), close);
            compare(dnd.nextItemInFocusChain(false), settings);
            compare(close.nextItemInFocusChain(true), settings);
            compare(settings.nextItemInFocusChain(true), dnd);

            fakePresentation.operationBusy = true;
            tryCompare(status, "visible", true);
            verify(status.x >= title.x + title.width);
            verify(status.x + status.width <= dnd.x);
            fakePresentation.operationBusy = false;
            compare(dnd.Accessible.name, "Turn on Do Not Disturb");
            verify(dnd.Accessible.description.indexOf(
                       "critical banners remain visible") >= 0);

            // Emit local signals; the test must not synthesize desktop input.
            dnd.clicked();
            compare(fakeQuieting.requests, 1);
            tryCompare(dnd, "checked", true);
            compare(dnd.Accessible.name, "Turn off Do Not Disturb");
            dnd.clicked();
            compare(fakeQuieting.requests, 2);
            tryCompare(dnd, "checked", false);
            settings.clicked();
            compare(fakeSettingsLauncher.launches, 1);
        }

        function test_transportStartFailureRetryThenRecovery() {
            fakeQuieting.canToggle = false;
            fakeQuieting.unavailable = true;
            fakeQuieting.statusText = "Do Not Disturb setting unavailable";
            fakeQuieting.errorText = "settings session D-Bus is not connected";
            const center = createTemporaryObject(centerComponent, testRoot);
            verify(center !== null);
            center.width = 384;
            center.height = 284;
            center.visible = true;
            const dnd = findChild(center, "notificationDoNotDisturbButton");
            const action = findChild(center, "notificationQuietingStateAction");
            const status = findChild(center, "notificationQuietingStatus");
            tryCompare(action, "visible", true);
            verify(!dnd.enabled);
            compare(action.text, "Retry");
            compare(status.Accessible.role, Accessible.AlertMessage);
            action.clicked();
            compare(fakeQuieting.retries, 1);

            fakeQuieting.unavailable = false;
            fakeQuieting.statusText = "Loading Do Not Disturb setting…";
            tryCompare(action, "visible", false);
            verify(!dnd.enabled);
            fakeQuieting.canToggle = true;
            fakeQuieting.statusText = "";
            tryCompare(dnd, "enabled", true);
        }

        function test_conflictLossAndConfirmedFailureStayTruthful() {
            fakeQuieting.canToggle = false;
            fakeQuieting.conflict = true;
            fakeQuieting.statusText = "Changed elsewhere; current value reloaded";
            const center = createTemporaryObject(centerComponent, testRoot);
            center.width = 384;
            center.height = 284;
            center.visible = true;
            const dnd = findChild(center, "notificationDoNotDisturbButton");
            const action = findChild(center, "notificationQuietingStateAction");
            const status = findChild(center, "notificationQuietingStatus");
            tryCompare(action, "visible", true);
            compare(action.text, "Apply my choice");

            fakeQuieting.conflict = false;
            fakeQuieting.unavailable = true;
            fakeQuieting.statusText = "Last confirmed: Off";
            tryCompare(action, "text", "Retry");
            verify(!dnd.enabled);

            fakeQuieting.unavailable = false;
            fakeQuieting.canToggle = true;
            fakeQuieting.statusText = "";
            fakeQuieting.errorText = "durable save failed";
            tryCompare(status, "text", "durable save failed");
            compare(status.Accessible.role, Accessible.AlertMessage);
            verify(!action.visible);
            verify(dnd.enabled);
        }
    }
}
