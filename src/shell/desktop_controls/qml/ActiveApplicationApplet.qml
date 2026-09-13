// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// GNOME-style active application indicator: the focused task-list row's
// application name and icon; the popup offers Minimize and Close through the
// task-list facade with the row's displayed revision.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready
    readonly property bool hasWindow: ready && Boolean(access.hasActiveWindow)
    readonly property string applicationName: hasWindow ? String(access.applicationName) : ""

    // The task row the open popup was opened for; empty while it is closed.
    property string popupTaskId: ""
    property string popupRevision: ""

    // AGENT-GUARD: Minimize and Close act on the facade's CURRENT row, so an
    // open popup must never outlive the task id and displayed revision it was
    // opened for, or its rows would act on a replacement task. The
    // controller's id/revision fence stays the dispatch authority; this only
    // retires stale presentation (docs/wiki/shell/desktop-controls.md).
    function popupTaskCurrent() {
        return root.hasWindow && root.popupTaskId.length > 0
            && String(root.access.taskId) === root.popupTaskId
            && String(root.access.revision) === root.popupRevision
    }

    function retireStalePopup() {
        if (actions.visible && !root.popupTaskCurrent())
            actions.close()
    }

    function dispatchIntent(intent) {
        if (!root.popupTaskCurrent()) {
            actions.close()
            return
        }
        if (intent())
            actions.close()
    }

    objectName: "activeApplicationApplet"
    implicitWidth: summary.implicitWidth
    implicitHeight: 28

    // Runs on the publishing turn: the facade emits before any input event can
    // reach a row of the now-stale popup.
    Connections {
        target: root.access
        ignoreUnknownSignals: true
        function onStateChanged() {
            root.retireStalePopup()
        }
    }

    T.ToolButton {
        id: summary
        objectName: "activeApplicationSummary"
        anchors.fill: parent
        enabled: root.hasWindow
        focusPolicy: Qt.TabFocus
        hoverEnabled: true
        padding: Tokens.space["1"]
        implicitWidth: Math.max(28, summaryRow.implicitWidth + leftPadding + rightPadding)
        text: ""

        Accessible.role: Accessible.Button
        Accessible.name: root.ready ? String(root.access.accessibleName)
                                    : qsTr("Active application is unavailable")
        Accessible.description: root.ready ? String(root.access.accessibleDescription) : ""

        function openActions() {
            if (!root.hasWindow)
                return
            root.popupTaskId = String(root.access.taskId)
            root.popupRevision = String(root.access.revision)
            actions.open()
        }

        onClicked: openActions()
        Keys.onReturnPressed: openActions()
        Keys.onEnterPressed: openActions()
        Accessible.onPressAction: openActions()

        contentItem: RowLayout {
            id: summaryRow
            spacing: Tokens.space["2"]

            ShellIcons.Icon {
                objectName: "activeApplicationIcon"
                name: root.hasWindow ? String(root.access.iconName) : "preferences-system-windows"
                size: Math.max(0, Math.min(18, root.height - Tokens.space["2"]))
                color: root.hasWindow ? Tokens.fg.default : Tokens.fg.muted
                symbolic: !root.hasWindow
                fallbackText: root.hasWindow ? root.applicationName : qsTr("Desktop")
                Accessible.ignored: true
            }

            Text {
                objectName: "activeApplicationName"
                visible: !root.vertical
                text: root.hasWindow ? root.applicationName : qsTr("Desktop")
                color: root.hasWindow ? Tokens.fg.default : Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Accessible.ignored: true
            }
        }

        background: Rectangle {
            radius: Tokens.radius.m
            color: summary.down ? Tokens.state.pressed
                 : summary.hovered ? Tokens.state.hover : "transparent"
            C.FocusRing { anchors.fill: parent; control: summary }
        }
    }

    ControlPopupFrame {
        id: actions
        objectName: "activeApplicationPopup"
        heading: root.applicationName
        feedback: root.ready && root.access.feedbackPresent ? String(root.access.feedback) : ""
        initialFocusItem: minimizeRow
        onClosed: {
            root.popupTaskId = ""
            root.popupRevision = ""
            summary.forceActiveFocus(Qt.PopupFocusReason)
        }

        C.Label {
            objectName: "activeApplicationTitle"
            Layout.fillWidth: true
            text: root.hasWindow ? String(root.access.title) : ""
            muted: true
            elide: Text.ElideRight
            maximumLineCount: 2
        }

        MenuRow {
            id: minimizeRow
            objectName: "activeApplicationMinimize"
            Layout.fillWidth: true
            iconName: "window-minimize"
            text: root.hasWindow && Boolean(root.access.minimized) ? qsTr("Minimized") : qsTr("Minimize")
            detail: qsTr("Hide the window without closing it")
            enabled: root.hasWindow && Boolean(root.access.canManage) && !Boolean(root.access.minimized)
            Keys.onDownPressed: closeRow.forceActiveFocus(Qt.TabFocusReason)
            onActivated: root.dispatchIntent(function() { return root.access.minimize() })
        }

        MenuRow {
            id: closeRow
            objectName: "activeApplicationClose"
            Layout.fillWidth: true
            iconName: "window-close"
            text: qsTr("Close")
            detail: root.hasWindow && root.access.windowCount > 1
                    ? qsTr("Closes %1 windows").arg(root.access.windowCount)
                    : qsTr("Asks the application to close its window")
            destructive: true
            enabled: root.hasWindow && Boolean(root.access.canManage)
            Keys.onUpPressed: minimizeRow.forceActiveFocus(Qt.TabFocusReason)
            onActivated: root.dispatchIntent(function() { return root.access.close() })
        }
    }
}
