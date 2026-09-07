// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
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
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    readonly property int resolvedDockTileSize: Math.max(56, Math.min(64, dockTileSize))
    // A container's user-chosen accent color (see ContainerAppearance)
    // recolors its dock/panel icon; empty for every standalone window and
    // every container that never picked a color, in which case the icon
    // keeps the ordinary enabled/disabled token color.
    readonly property color resolvedIconColor: String(entry.colorHex ?? "").length > 0
        ? entry.colorHex
        : (enabled ? Tokens.fg.default : Tokens.fg.disabled)

    objectName: "taskListEntryButton"
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    // AGENT-GUARD: dock tiles reserve their full hover envelope before the
    // icon lifts. Do not scale the delegate itself: GridLayout would retain
    // the old bounds and clip or overlap adjacent accessible hit targets.
    implicitWidth: dockMode ? resolvedDockTileSize
                            : (vertical ? 32 : Math.max(84, Math.min(168, rowLayout.implicitWidth + 12)))
    implicitHeight: dockMode ? resolvedDockTileSize : 28

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

    T.ToolTip {
        id: dockTooltip
        objectName: "taskListEntryTooltip"
        visible: button.dockMode && button.hovered
        text: button.entry.accessibleName
        delay: Tokens.motion.short
        popupType: T.Popup.Window
    }

    // Right-click opens the same context actions the Menu key exposes.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: contextMenu.popup()
    }

    contentItem: Item {
        RowLayout {
            id: rowLayout
            anchors.fill: parent
            visible: !button.dockMode
            spacing: Tokens.space["2"]

            ShellIcons.Icon {
                objectName: "taskListEntryIcon"
                name: String(button.entry.iconName ?? "")
                size: 18
                color: button.resolvedIconColor
                symbolic: false
                fallbackText: button.entry.applicationName
                Accessible.ignored: true
            }

            Text {
                id: titleText
                objectName: "taskListEntryTitle"
                Layout.fillWidth: true
                visible: !button.vertical
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
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                font.bold: true
                Accessible.ignored: true
            }

            Text {
                objectName: "taskListEntryCountBadge"
                visible: !button.vertical && button.entry.kind === "container"
                text: visible ? qsTr("×%1").arg(button.entry.windowCount) : ""
                color: Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                Accessible.ignored: true
            }
        }

        ShellIcons.Icon {
            id: dockIcon
            objectName: "taskListDockEntryIcon"
            visible: button.dockMode
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            name: String(button.entry.iconName ?? "")
            size: 40
            color: button.resolvedIconColor
            symbolic: false
            fallbackText: button.entry.applicationName
            scale: button.hovered && !button.reducedMotion ? 1.08 : 1.0
            transformOrigin: Item.Center
            property real hoverLift: button.hovered && !button.reducedMotion ? -3 : 0
            transform: Translate { y: dockIcon.hoverLift }
            Accessible.ignored: true

            // QST reduces the published durations when accessibility reduced
            // motion is enabled; this presentation owns no separate setting.
            Behavior on hoverLift {
                NumberAnimation { duration: Tokens.motion.short }
            }
            Behavior on scale {
                NumberAnimation { duration: Tokens.motion.short }
            }
        }

        // Every dock item represents an existing task row, never a synthetic
        // pin. A dot therefore reports current running-task truth only.
        Rectangle {
            objectName: "taskListRunningIndicator"
            visible: button.dockMode
            width: 4
            height: 4
            radius: width / 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Tokens.space["1"]
            color: button.entry.active ? Tokens.fg.default : Tokens.fg.muted
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: button.dockMode ? Tokens.radius.l : Tokens.radius.m
        color: button.down ? Tokens.state.pressed
             : button.hovered ? Tokens.state.hover
             : button.entry.active ? Tokens.bg.raised
             : "transparent"
        border.color: Tokens.outline.divider
        border.width: button.dockMode && !button.down && !button.hovered && !button.entry.active
                      ? 0 : Tokens.space["1"] / 2
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
        popupType: T.Popup.Window

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
            text: button.entry.minimized ? qsTr("Restore") : qsTr("Minimize")
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
        T.MenuItem {
            objectName: "taskListContextRaise"
            text: qsTr("Raise")
            enabled: button.access !== null && button.access.canManage
                     && !button.entry.pending
            onTriggered: button.access.raiseTask(
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
