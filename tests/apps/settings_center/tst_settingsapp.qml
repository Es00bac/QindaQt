// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/apps/settings_center" as SettingsApp

Item {
    id: root
    width: 560
    height: 420

    QtObject {
        id: quieting
        property bool enabled: false
        property bool canToggle: false
        property bool conflict: false
        property bool unavailable: false
        property string statusText: "Loading Do Not Disturb setting…"
        property string errorText: ""
        property int requests: 0
        property int retries: 0
        property int applies: 0
        function requestSet(value) { ++requests; enabled = value; return true; }
        function retry() { ++retries; }
        function applyMyChoice() { ++applies; return true; }
    }

    Component {
        id: pageComponent
        SettingsApp.NotificationsPage { quietingSettings: quieting }
    }

    TestCase {
        name: "SettingsNotificationsPage"
        when: windowShown

        function init() {
            quieting.enabled = false;
            quieting.canToggle = false;
            quieting.conflict = false;
            quieting.unavailable = false;
            quieting.statusText = "Loading Do Not Disturb setting…";
            quieting.errorText = "";
            quieting.requests = 0;
            quieting.retries = 0;
            quieting.applies = 0;
        }

        function test_loadingReadySavingAndAccessibility() {
            const page = createTemporaryObject(pageComponent, root);
            verify(page !== null);
            const toggle = findChild(page, "settingsDoNotDisturbSwitch");
            const status = findChild(page, "settingsQuietingStatus");
            const close = findChild(page, "settingsCloseButton");
            verify(toggle !== null);
            verify(status !== null);
            verify(close !== null);
            verify(!toggle.enabled);
            compare(status.text, "Loading Do Not Disturb setting…");
            compare(status.Accessible.role, Accessible.StaticText);
            compare(toggle.Accessible.role, Accessible.CheckBox);
            compare(toggle.Accessible.name, "Do Not Disturb");
            verify(toggle.Accessible.description.indexOf(
                       "privacy permits") >= 0);
            compare(toggle.KeyNavigation.backtab, close);

            quieting.canToggle = true;
            quieting.statusText = "";
            tryVerify(function() { return toggle.enabled; });
            toggle.clicked();
            compare(quieting.requests, 1);
            verify(quieting.enabled);
            quieting.canToggle = false;
            quieting.statusText = "Saving…";
            compare(status.text, "Saving…");
            verify(!toggle.enabled);
        }

        function test_conflictAndUnavailableRoutesPreserveFocusChain() {
            const page = createTemporaryObject(pageComponent, root);
            const toggle = findChild(page, "settingsDoNotDisturbSwitch");
            const conflict = findChild(page, "settingsConflictApplyButton");
            const retry = findChild(page, "settingsRetryButton");
            const close = findChild(page, "settingsCloseButton");
            const status = findChild(page, "settingsQuietingStatus");
            quieting.conflict = true;
            quieting.statusText = "Changed elsewhere; current value reloaded";
            tryVerify(function() { return conflict.visible; });
            compare(toggle.KeyNavigation.tab, conflict);
            compare(conflict.KeyNavigation.tab, close);
            compare(status.Accessible.role, Accessible.AlertMessage);
            conflict.clicked();
            compare(quieting.applies, 1);

            quieting.conflict = false;
            quieting.unavailable = true;
            quieting.statusText = "Last confirmed: On";
            tryVerify(function() { return retry.visible; });
            compare(toggle.KeyNavigation.tab, retry);
            retry.clicked();
            compare(quieting.retries, 1);
        }
    }
}
