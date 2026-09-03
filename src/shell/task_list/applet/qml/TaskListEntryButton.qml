// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// One task-list strip row: standalone window or collapsed container. The row
// renders only controller-projected values and dispatches intents with the
// exact generationRevision it displays, so the T0 stale-id arbitration can
// refuse actions against a generation the user no longer sees.
//
// AGENT-CONTRACT: all visuals derive from semantic QST-1 roles (QindaQt.Tokens)
// and the QindaQt.Controls focus ring; no applet-owned palette or fallback
// colors. The QQC2 ToolButton base supplies button behavior only (press,
// clicked, enabled) — the same shape as QindaQt.Controls' own primitives.
T.ToolButton {
    id: button

    required property var entry
    required property var access
    property bool vertical: false

    objectName: "taskListEntryButton"
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    implicitWidth: vertical ? 40 : Math.max(96, Math.min(168, rowLayout.implicitWidth + 16))
    implicitHeight: vertical ? 56 : 28

    // AGENT-GUARD: the controller re-checks capability, generation, and
    // pending fences on every call; this enabled binding is presentation
    // honesty (busy/unavailable controls look disabled), never the gate.
    enabled: access !== null && access.phaseText === "ready" && !entry.pending

    function activate() {
        if (enabled)
            access.activateTask(entry.taskId, entry.generationRevision)
    }

    onClicked: activate()
    Accessible.onPressAction: activate()

    // QQC2's Basic ToolButton handles Space but ignores Return/Enter; wire
    // them to the same activation path so the keyboard contract is complete.
    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onMenuPressed: contextMenu.popup()

    Accessible.role: Accessible.Button
    Accessible.name: entry.accessibleName
    Accessible.description: entry.pending
        ? qsTr("Operation pending for this window")
        : qsTr("Press to activate. Press the Menu key for window actions.")

    // Right-click opens the same context actions the Menu key exposes.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: contextMenu.popup()
    }

    contentItem: RowLayout {
        id: rowLayout
        spacing: Tokens.space["2"]

        // Typed icon placeholder: a deterministic one-letter badge. No icon
        // seam exists in this tree yet; the badge keeps the row shape stable.
        Rectangle {
            objectName: "taskListEntryIcon"
            implicitWidth: 18
            implicitHeight: 18
            radius: Tokens.radius.s
            color: Tokens.bg.highest
            border.color: Tokens.outline.strong

            Text {
                anchors.centerIn: parent
                text: button.entry.iconText
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                Accessible.ignored: true
            }
        }

        Text {
            id: titleText
            objectName: "taskListEntryTitle"
            Layout.fillWidth: true
            text: button.entry.title.length > 0
                  ? button.entry.title : button.entry.applicationName
            color: button.enabled ? Tokens.fg.default : Tokens.fg.disabled
            elide: Text.ElideRight
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            Accessible.ignored: true
        }

        // Demand-attention truth is text, never color-only.
        Text {
            objectName: "taskListEntryUrgentBadge"
            visible: button.entry.urgent
            text: "!"
            color: Tokens.status.warning.foreground
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            font.bold: true
            Accessible.ignored: true
        }

        Text {
            objectName: "taskListEntryCountBadge"
            visible: button.entry.kind === "container"
            text: visible ? qsTr("%1 windows").arg(button.entry.windowCount) : ""
            color: Tokens.fg.muted
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: button.down ? Tokens.state.pressed
             : button.hovered ? Tokens.state.hover
             : button.entry.active ? Tokens.bg.raised
             : "transparent"
        border.color: Tokens.outline.divider
        border.width: Tokens.space["1"] / 2
        opacity: button.entry.minimized ? 0.7 : 1.0

        C.FocusRing {
            objectName: "taskListEntryFocusRing"
            anchors.fill: parent
            control: button
        }
    }

    // QindaQt.Controls ships no menu primitive yet; the context menu uses the
    // QQC2 style palette and owns no applet-side colors.
    T.Menu {
        id: contextMenu
        objectName: "taskListContextMenu"

        T.MenuItem {
            objectName: "taskListContextActivate"
            text: qsTr("Activate")
            enabled: button.access !== null && button.access.canActivate
                     && !button.entry.pending
            onTriggered: button.access.activateTask(
                             button.entry.taskId, button.entry.generationRevision)
        }
        T.MenuItem {
            objectName: "taskListContextMinimize"
            text: qsTr("Minimize")
            enabled: button.access !== null && button.access.canManage
                     && !button.entry.pending
            onTriggered: button.access.minimizeTask(
                             button.entry.taskId, button.entry.generationRevision)
        }
        T.MenuItem {
            objectName: "taskListContextClose"
            text: qsTr("Close")
            enabled: button.access !== null && button.access.canManage
                     && !button.entry.pending
            onTriggered: button.access.closeTask(
                             button.entry.taskId, button.entry.generationRevision)
        }
        T.MenuSeparator {
            visible: button.entry.kind === "container"
        }
        T.MenuItem {
            // The Ungroup arm of the shell-owned container close policy; it
            // maps to the T1 releaseContainer operation.
            objectName: "taskListContextUngroup"
            visible: button.entry.kind === "container"
            text: qsTr("Ungroup")
            enabled: button.access !== null && button.access.canManage
                     && !button.entry.pending
            onTriggered: button.access.ungroupContainer(
                             button.entry.taskId, button.entry.generationRevision)
        }
    }
}
