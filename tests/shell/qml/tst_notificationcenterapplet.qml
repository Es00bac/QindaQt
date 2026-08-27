// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as ShellComponents

Item {
    id: testRoot
    width: 360
    height: 180

    property var theme: ({
        "cornerRadius": 8,
        "colors": {
            "surface": "#222624",
            "surfaceRaised": "#2c312e",
            "text": "#f2f1eb",
            "textMuted": "#a9afa9",
            "accent": "#8fc8b7",
            "accentText": "#10201b"
        }
    })

    QtObject {
        id: fakeAccess
        property bool centerOpen: false
        property int toggleCalls: 0
        function toggle() { ++toggleCalls; }
    }

    Component {
        id: appletComponent
        ShellComponents.NotificationCenterApplet {
            access: fakeAccess
            theme: testRoot.theme
        }
    }

    Component {
        id: dispatcherComponent
        ShellComponents.BuiltinAppletContent {
            width: 100
            height: 40
            applet: ({
                "plugin": "notification-center",
                "settings": {},
                "runtime": {
                    "ready": true,
                    "entryPoint": "qindaqt.applets.notification-center"
                }
            })
            theme: testRoot.theme
            liveApplets: true
            notificationCenterAppletAccess: fakeAccess
        }
    }

    Component {
        id: clockChipComponent
        ShellComponents.AppletChip {
            applet: testRoot.runtimeApplet("qindaqt.applets.clock", true)
            theme: testRoot.theme
            liveApplets: true
            notificationCenterAppletAccess: fakeAccess
        }
    }

    function runtimeApplet(entryPoint, ready) {
        return {
            "plugin": entryPoint === "qindaqt.applets.notification-center"
                      ? "notification-center" : "clock",
            "settings": {},
            "runtime": {
                "ready": ready,
                "entryPoint": entryPoint
            }
        };
    }

    TestCase {
        name: "NotificationCenterApplet"
        when: windowShown

        function init() {
            fakeAccess.centerOpen = false;
            fakeAccess.toggleCalls = 0;
        }

        function test_narrowActivationAndAccessibility() {
            const applet = createTemporaryObject(appletComponent, testRoot);
            verify(applet !== null);
            compare(applet.Accessible.role, Accessible.Button);
            compare(applet.Accessible.name, "Open notification center");
            applet.activate();
            compare(fakeAccess.toggleCalls, 1);

            fakeAccess.centerOpen = true;
            tryCompare(applet.Accessible, "name", "Close notification center");
        }

        function test_nullAccessIsDisabled() {
            const applet = createTemporaryObject(
                               appletComponent, testRoot, { "access": null });
            verify(applet !== null);
            verify(!applet.enabled);
            compare(applet.Accessible.name, "Notifications unavailable");
            applet.activate();
            compare(fakeAccess.toggleCalls, 0);
        }

        function test_dispatcherMapsRegisteredEntryPoint() {
            const dispatcher = createTemporaryObject(dispatcherComponent, testRoot);
            verify(dispatcher !== null);
            verify(dispatcher.hasLiveContent);
            verify(dispatcher.notificationCenterReady);
            verify(!dispatcher.clockReady);
            const applet = findChild(dispatcher, "notificationCenterApplet");
            verify(applet !== null);
            verify(applet.visible);
            applet.activate();
            compare(fakeAccess.toggleCalls, 1);
        }


        function test_dispatcherMapsEveryCompiledRenderer() {
            const clockDispatcher = createTemporaryObject(
                                      dispatcherComponent, testRoot,
                                      { "applet": testRoot.runtimeApplet(
                                          "qindaqt.applets.clock", true) });
            verify(clockDispatcher !== null);
            verify(clockDispatcher.hasLiveContent);
            verify(clockDispatcher.clockReady);
            verify(!clockDispatcher.notificationCenterReady);
            const clock = findChild(clockDispatcher, "clockApplet");
            const notifications = findChild(clockDispatcher,
                                            "notificationCenterApplet");
            verify(clock !== null);
            verify(clock.visible);
            verify(notifications !== null);
            verify(!notifications.visible);
        }

        function test_dispatcherFailsClosedForUnregisteredOrUnavailableContent() {
            const unknown = createTemporaryObject(
                                dispatcherComponent, testRoot,
                                { "applet": testRoot.runtimeApplet(
                                    "qindaqt.applets.not-registered", true) });
            verify(unknown !== null);
            verify(!unknown.hasLiveContent);
            verify(!unknown.clockReady);
            verify(!unknown.notificationCenterReady);

            const unready = createTemporaryObject(
                                dispatcherComponent, testRoot,
                                { "applet": testRoot.runtimeApplet(
                                    "qindaqt.applets.notification-center",
                                    false) });
            verify(unready !== null);
            verify(!unready.hasLiveContent);

            const preview = createTemporaryObject(
                                dispatcherComponent, testRoot,
                                { "liveApplets": false });
            verify(preview !== null);
            verify(!preview.hasLiveContent);
        }

        function test_clockChipHasNoNotificationActivationSurface() {
            const chip = createTemporaryObject(clockChipComponent, testRoot);
            verify(chip !== null);
            verify(findChild(chip, "clockApplet") !== null);
            compare(findChild(chip, "notificationCenterToggle"), null);
            compare(fakeAccess.toggleCalls, 0);
        }
    }
}
