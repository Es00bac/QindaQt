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

    ListModel { id: popupItems }
    ListModel { id: historyItems }

    QtObject {
        id: fakePresentation
        property var activeModel: activeItems
        property var popupModel: popupItems
        property var historyModel: historyItems
        property bool centerOpen: false
        property bool doNotDisturbEnabled: false
        property int doNotDisturbChanges: 0
        property bool operationBusy: false
        property string operationErrorText: ""

        function setCenterOpen(open) { centerOpen = open; }
        onDoNotDisturbEnabledChanged: ++doNotDisturbChanges
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
        property int requests: 0
        property int retries: 0
        property int applies: 0
        function requestSet(value) { ++requests; enabled = value; return true; }
        function retry() { ++retries; }
        function applyMyChoice() { ++applies; return true; }
    }

    QtObject {
        id: fakeSettingsLauncher
        property int launches: 0
        function openNotifications() { ++launches; return true; }
    }

    property var theme: ({
        "cornerRadius": 10,
        "colors": {
            "surface": "#222624",
            "surfaceRaised": "#2c312e",
            "border": "#3c433f",
            "text": "#f2f1eb",
            "textMuted": "#a9afa9",
            "accent": "#8fc8b7",
            "accentText": "#10201b",
            "danger": "#f07c76"
        }
    })

    function appendFixture(model, notificationId) {
        model.append({
            "notificationId": notificationId,
            "applicationName": "QindaQt Test",
            "summary": "Plain summary",
            "body": "<b>Must remain plain text</b>",
            "urgency": 1,
            "actions": [],
            "active": true
        });
    }

    Component.onCompleted: {
        appendFixture(activeItems, 7);
        appendFixture(popupItems, 8);
    }

    Component {
        id: cardComponent
        ShellComponents.NotificationCard {
            width: 400
            presentation: fakePresentation
            theme: testRoot.theme
            notificationId: 7
            applicationName: "QindaQt Test"
            summary: "Plain summary"
            body: "<b>Must remain plain text</b>"
            urgency: 1
            actions: [
                { "key": "one", "label": "A very long primary action label" },
                { "key": "two", "label": "Another very long action label" },
                { "key": "three", "label": "Overflow action" },
                { "key": "four", "label": "Another overflow action" }
            ]
            active: true
        }
    }

    Component {
        id: popupComponent
        ShellComponents.NotificationPopupStack {
            presentation: fakePresentation
            theme: testRoot.theme
        }
    }

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
        name: "NotificationSurfaces"
        when: windowShown

        function init() {
            fakePresentation.centerOpen = false;
            fakePresentation.doNotDisturbEnabled = false;
            fakePresentation.doNotDisturbChanges = 0;
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
            fakeQuieting.applies = 0;
            fakeSettingsLauncher.launches = 0;
        }

        function test_centerProvidesWindowScopedCancelPath() {
            const center = createTemporaryObject(centerComponent, testRoot);
            verify(center !== null);
            const cancelShortcut = findChild(
                                     center,
                                     "notificationCenterCloseShortcut");
            const closeButton = findChild(center,
                                          "notificationCenterCloseButton");
            verify(cancelShortcut !== null);
            verify(closeButton !== null);
            compare(cancelShortcut.context, Qt.WindowShortcut);
            compare(cancelShortcut.enabled, false);

            fakePresentation.centerOpen = true;
            center.visible = true;
            tryCompare(cancelShortcut, "enabled", true);
            closeButton.forceActiveFocus(Qt.ActiveWindowFocusReason);
            // The offscreen platform cannot activate this child Window, but
            // the focus target itself must still accept the seeded focus.
            verify(closeButton.focus);

            // Emit the Shortcut signal directly: the offscreen test proves the
            // close route without registering or synthesizing desktop input.
            cancelShortcut.activated();
            compare(fakePresentation.centerOpen, false);
        }

        function test_componentsInstantiateOffscreen() {
            const card = createTemporaryObject(cardComponent, testRoot);
            verify(card !== null);
            const summary = findChild(card, "notificationSummary");
            const body = findChild(card, "notificationBody");
            verify(summary !== null);
            verify(body !== null);
            compare(summary.textFormat, Text.PlainText);
            compare(body.textFormat, Text.PlainText);
            compare(body.text, "<b>Must remain plain text</b>");
            const actionRow = findChild(card, "notificationActionRow");
            const primaryAction = findChild(card, "notificationPrimaryAction");
            const more = findChild(card, "notificationMoreActions");
            const dismiss = findChild(card, "notificationDismiss");
            verify(actionRow !== null);
            verify(primaryAction !== null);
            verify(more !== null);
            verify(dismiss !== null);
            verify(more.visible);
            verify(dismiss.visible);
            verify(dismiss.x + dismiss.width <= actionRow.width);
            fakePresentation.operationBusy = true;
            tryCompare(primaryAction, "enabled", false);
            compare(more.enabled, false);
            compare(dismiss.enabled, false);
            fakePresentation.operationBusy = false;
            tryCompare(dismiss, "enabled", true);

            const compactCard = createTemporaryObject(
                                  cardComponent, testRoot, { "width": 240 });
            verify(compactCard !== null);
            const compactRow = findChild(compactCard, "notificationActionRow");
            const compactMore = findChild(compactCard,
                                          "notificationMoreActions");
            const compactDismiss = findChild(compactCard,
                                             "notificationDismiss");
            verify(compactMore.visible);
            verify(compactDismiss.x + compactDismiss.width <= compactRow.width);

            fakePresentation.operationBusy = true;
            const popup = createTemporaryObject(popupComponent, testRoot);
            verify(popup !== null);
            compare(popup.visible, false);
            const popupStatus = findChild(popup, "notificationOperationStatus");
            verify(popupStatus !== null);
            verify(popupStatus.visible);
            compare(popupStatus.text, "Working…");
            compare(popupStatus.textFormat, Text.PlainText);
            compare(popupStatus.Accessible.role, Accessible.StaticText);
            compare(popupStatus.Accessible.name,
                    "Notification operation in progress");

            const center = createTemporaryObject(centerComponent, testRoot);
            verify(center !== null);
            compare(center.visible, false);
            const centerStatus = findChild(center, "notificationOperationStatus");
            verify(centerStatus !== null);
            verify(centerStatus.visible);
            compare(centerStatus.text, "Working…");

            fakePresentation.operationErrorText =
                "<b>Request failed safely</b>";
            tryCompare(popupStatus, "text", "<b>Request failed safely</b>");
            compare(popupStatus.textFormat, Text.PlainText);
            compare(popupStatus.Accessible.role, Accessible.AlertMessage);
            compare(popupStatus.Accessible.name,
                    "Notification operation failed: " +
                    "<b>Request failed safely</b>");

            fakePresentation.operationBusy = false;
            tryCompare(popupStatus, "visible", true);
            compare(popupStatus.text, "<b>Request failed safely</b>");
            fakePresentation.operationErrorText = "";
            tryCompare(popupStatus, "visible", false);
            compare(centerStatus.visible, false);
        }
    }
}
