// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtTest
import QindaQt.Shell.StatusNotifier.Tests 1.0 as Harness
import "../../../../../src/shell/qml" as ShellComponents

// Production dispatcher row: the real PanelAppletRow → AppletChip →
// BuiltinAppletContent chain hosting the compiled StatusNotifier module,
// driven by a scripted controller fake. Runs under QT_FATAL_WARNINGS=1 with
// host display and bus variables unset (registered in CMake).
Item {
    id: root
    width: 560
    height: 220

    readonly property bool harnessReady: Harness.Harness.ready

    property var theme: ({
        "cornerRadius": 6,
        "colors": {
            "surface": "#222624", "surfaceRaised": "#2c312e",
            "border": "#3c433f", "text": "#f2f1eb",
            "textMuted": "#a9afa9", "accent": "#8fc8b7"
        }
    })

    QtObject {
        id: fakeAccess

        property string phaseText: "ready"
        property string phaseReasonText: ""
        property bool watcherLive: true
        property var itemRows: []
        property int itemCount: 0
        property int presentedCount: 0
        property int overflowCount: 0
        property string overflowText: ""
        property bool readGranted: true
        property bool activateGranted: true
        property int iconSize: 22
        property bool feedbackPresent: false
        property string feedback: ""
        property string feedbackStatus: "error"

        property int activateCalls: 0
        property int openContextMenuCalls: 0
        property int menuRowsForCalls: 0
        property var lastActivateArgs: null
        property var scriptedMenuRows: []

        function activateItem(uniqueName, objectPath, generation) {
            ++activateCalls
            lastActivateArgs = [uniqueName, objectPath, generation]
            return true
        }
        function secondaryActivateItem(uniqueName, objectPath, generation) {
            return true
        }
        function openContextMenu(uniqueName, objectPath, generation) {
            ++openContextMenuCalls
            return true
        }
        function menuRowsFor(uniqueName, objectPath, generation) {
            ++menuRowsForCalls
            return scriptedMenuRows
        }
        function acknowledgeDegraded() {}
        function clearFeedback() {}
    }

    function makeRow(uniqueName, generation) {
        return {
            uniqueName: uniqueName,
            objectPath: "/StatusNotifierItem",
            generation: generation,
            identity: "org.qindaqt." + uniqueName,
            title: "Item " + uniqueName,
            accessibleName: "Item " + uniqueName,
            accessibleDescription: "",
            accessibleStatusText: "active",
            needsAttention: false,
            active: true,
            hasMenu: true,
            menuEntryCount: 1,
            keyboardActivateText: "Enter or Space",
            keyboardContextMenuText: "Shift+F10 or Menu key",
            secondaryActivatePointerOnly: true,
            iconDataUrl: "",
            iconIsPlaceholder: true
        }
    }

    TextInput {
        id: beforePanel
        objectName: "beforeStatusNotifierPanel"
        text: "before"
        activeFocusOnTab: true
    }

    ShellComponents.PanelAppletRow {
        id: panelRow
        objectName: "productionStatusNotifierPanelRow"
        anchors.left: parent.left
        anchors.top: beforePanel.bottom
        width: 520
        height: 40
        zone: "end"
        liveApplets: true
        theme: root.theme
        statusNotifierAppletAccess: fakeAccess
        panel: ({
            "applets": [{
                "id": "status-notifier", "plugin": "status-notifier",
                "settings": { "zone": "end" },
                "runtime": {
                    "ready": true,
                    "entryPoint": "qindaqt.applets.status-notifier"
                }
            }]
        })
    }

    TestCase {
        name: "StatusNotifierProductionPanelKeyboard"
        when: windowShown && root.harnessReady

        function init() {
            fakeAccess.phaseText = "ready"
            fakeAccess.itemRows = [root.makeRow(":1.42", 3)]
            fakeAccess.itemCount = 1
            fakeAccess.presentedCount = 1
            fakeAccess.overflowCount = 0
            fakeAccess.overflowText = ""
            fakeAccess.activateGranted = true
            fakeAccess.activateCalls = 0
            fakeAccess.openContextMenuCalls = 0
            fakeAccess.menuRowsForCalls = 0
            fakeAccess.lastActivateArgs = null
            fakeAccess.scriptedMenuRows = [{
                depth: 0, kind: "item", label: "Open",
                enabled: true, visible: true, hasChildren: false
            }]
        }

        // Tab enters the panel strip, Return dispatches exactly one
        // generation-fenced activation, and the delegate exposes its
        // accessible button truth.
        function test_a_tabReachesDelegateAndReturnActivates() {
            const applet = findChild(panelRow, "statusNotifierApplet")
            verify(applet !== null)
            const repeater = findChild(applet, "statusNotifierStripRepeater")
            verify(repeater !== null)
            tryCompare(repeater, "count", 1)
            const delegate = repeater.itemAt(0)
            verify(delegate !== null)
            compare(delegate.Accessible.role, Accessible.Button)
            compare(delegate.Accessible.name, "Item :1.42")

            beforePanel.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Tab)
            tryVerify(function() { return delegate.activeFocus })
            keyClick(Qt.Key_Return)
            tryVerify(function() { return fakeAccess.activateCalls === 1 })
            compare(fakeAccess.lastActivateArgs, [":1.42", "/StatusNotifierItem", 3])
        }

        // Shift+F10 opens the context menu as an independently focusable
        // popup WINDOW (the layer-shell panel itself rejects focus); Escape
        // closes it without dispatching anything. Runs last: the offscreen
        // backend cannot reactivate a parent after destroying a transient
        // native popup in the same process.
        function test_b_contextPopupIsWindowAndEscapeCloses() {
            const applet = findChild(panelRow, "statusNotifierApplet")
            verify(applet !== null)
            const repeater = findChild(applet, "statusNotifierStripRepeater")
            verify(repeater !== null)
            tryCompare(repeater, "count", 1)
            const delegate = repeater.itemAt(0)
            verify(delegate !== null)

            const popup = findChild(delegate, "statusNotifierContextPopup")
            verify(popup !== null)
            compare(popup.popupType, Popup.Window)

            beforePanel.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Tab)
            tryVerify(function() { return delegate.activeFocus })
            keyClick(Qt.Key_F10, Qt.ShiftModifier)
            tryVerify(function() { return fakeAccess.menuRowsForCalls === 1 })
            tryCompare(popup, "opened", true)

            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
            compare(fakeAccess.openContextMenuCalls, 0)
            compare(fakeAccess.activateCalls, 0)
        }
    }
}
