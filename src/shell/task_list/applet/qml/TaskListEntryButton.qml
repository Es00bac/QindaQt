// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts

// One task-list strip row: standalone window or collapsed container. The row
// renders only controller-projected values and dispatches intents with the
// exact generationRevision it displays, so the T0 stale-id arbitration can
// refuse actions against a generation the user no longer sees.
T.ToolButton {
    id: button

    required property var entry
    required property var access
    required property var theme
    property bool vertical: false

    readonly property var colors: theme.colors ?? ({})

    objectName: "taskListEntryButton"
    focusPolicy: Qt.TabFocus
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
        spacing: 6

        // Typed icon placeholder: a deterministic one-letter badge. No icon
        // seam exists in this tree yet; the badge keeps the row shape stable.
        Rectangle {
            objectName: "taskListEntryIcon"
            implicitWidth: 18
            implicitHeight: 18
            radius: 4
            color: button.colors.surfaceRaised ?? "#2c312e"
            border.color: button.colors.border ?? "#3c433f"

            T.Label {
                anchors.centerIn: parent
                text: button.entry.iconText
                color: button.colors.text ?? "#f2f1eb"
                font.pixelSize: 11
                Accessible.ignored: true
            }
        }

        T.Label {
            id: titleText
            objectName: "taskListEntryTitle"
            Layout.fillWidth: true
            text: button.entry.title.length > 0
                  ? button.entry.title : button.entry.applicationName
            color: button.enabled
                   ? (button.colors.text ?? "#f2f1eb")
                   : (button.colors.textMuted ?? "#a9afa9")
            elide: Text.ElideRight
            font.pixelSize: 11
            Accessible.ignored: true
        }

        // Demand-attention truth is text, never color-only.
        T.Label {
            objectName: "taskListEntryUrgentBadge"
            visible: button.entry.urgent
            text: "!"
            color: button.colors.warning ?? "#e5a84b"
            font.bold: true
            Accessible.ignored: true
        }

        T.Label {
            objectName: "taskListEntryCountBadge"
            visible: button.entry.kind === "container"
            text: visible ? qsTr("%1 windows").arg(button.entry.windowCount) : ""
            color: button.colors.textMuted ?? "#a9afa9"
            font.pixelSize: 10
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: button.theme.cornerRadius ?? 6
        color: button.entry.active
               ? (button.colors.surfaceRaised ?? "#2c312e")
               : "transparent"
        border.color: button.activeFocus
                      ? (button.colors.focus ?? button.colors.border ?? "#3c433f")
                      : (button.colors.border ?? "#3c433f")
        border.width: button.activeFocus ? 2 : 1
        opacity: button.entry.minimized ? 0.7 : 1.0
    }

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
