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
// AGENT-NOTE: the single exception is the instance-level `luna` dressing
// (ADR-0124): the Bliss profile opts individual taskbar applets in through
// their own profile settings, and those constants are presentation dressing,
// never the theme authority for any other host.
T.ToolButton {
    id: button

    required property var entry
    required property var access
    property bool vertical: false
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    property bool luna: false
    // Dock magnification factor for this tile (1.0 = rest). The strip computes
    // it from pointer proximity; transforms never touch layout bounds.
    property real dockZoomScale: 1.0
    // Strip-owned hover preview hooks (ADR-0119): the strip hosts the single
    // preview popup and supersedes hover state centrally.
    property var stripPreviewHover: null
    property var stripClosePreview: null
    // Drag-reorder visuals, driven by the strip's DragHandler state.
    property var stripMove: null
    property bool dragHeld: false
    property real dragShiftX: 0
    property real dragFollowX: 0
    readonly property int resolvedDockTileSize: Math.max(56, Math.min(64, dockTileSize))
    transform: Translate {
        x: button.dragShiftX + button.dragFollowX
    }
    // A container's user-chosen accent color (see ContainerAppearance)
    // recolors its dock/panel icon; empty for every standalone window and
    // every container that never picked a color, in which case the icon
    // keeps the ordinary enabled/disabled token color — or white on the
    // Luna dressing, where the bar supplies the dark ground.
    readonly property color resolvedIconColor: String(entry.colorHex ?? "").length > 0
        ? entry.colorHex
        : (luna ? "white" : (enabled ? Tokens.fg.default : Tokens.fg.disabled))

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

    onHoveredChanged: {
        if (stripPreviewHover !== null) {
            stripPreviewHover(index, hovered)
        }
    }

    onClicked: {
        if (stripClosePreview !== null) {
            stripClosePreview()
        }
        activate()
    }
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
        // The hover preview card replaces the tooltip when the preview seam
        // is present; an unavailable capture degrades to the preview card's
        // title-only form, never to a duplicate floating label.
        visible: button.dockMode && button.hovered
                 && !(button.access !== null && button.access.previewsEnabled)
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
                symbolic: button.entry.kind === "container"
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
                color: button.luna ? "white"
                      : button.enabled ? Tokens.fg.default : Tokens.fg.disabled
                elide: Text.ElideRight
                font.family: button.luna ? "Trebuchet MS" : Tokens.type.fontFamily
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
                color: button.luna ? "#cfe0f8" : Tokens.fg.muted
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
            symbolic: button.entry.kind === "container"
            fallbackText: button.entry.applicationName
            // AGENT-GUARD: bottom-anchored swell inside the tile's reserved
            // envelope — the icon grows upward from its rest bottom edge and
            // never shifts the delegate's layout bounds or hit target. The
            // hovered tile is also the falloff peak, so dockZoomScale
            // subsumes the former flat 1.08 hover bump.
            transformOrigin: Item.Bottom
            scale: button.reducedMotion ? 1.0
                  : button.dockMode ? Math.max(1.0, button.dockZoomScale)
                  : button.hovered ? 1.08 : 1.0
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

    // A Loader because `background` takes an Item: a `luna ? a : b` ternary
    // over Component ids yields a QQmlComponent, which the property rejects.
    background: Loader {
        sourceComponent: button.luna ? lunaBackground : classicBackground
    }

    Component {
        id: classicBackground
        Rectangle {
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
    }

    Component {
        id: lunaBackground
        Rectangle {
            radius: 3
            border.color: button.entry.active ? "#7aa7ee" : "#163a8c"
            border.width: 1
            opacity: button.entry.minimized ? 0.7 : 1.0
            gradient: Gradient {
                GradientStop { position: 0.0
                    color: button.down ? "#1b47a4"
                         : button.entry.active ? "#6d9ceb" : "#3a76dd" }
                GradientStop { position: 1.0
                    color: button.down ? "#163a8c"
                         : button.entry.active ? "#4a80d8" : "#2452b4" }
            }

            C.FocusRing {
                objectName: "taskListEntryLunaFocusRing"
                anchors.fill: parent
                control: button
            }
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
            objectName: "taskListContextUngroup"
            visible: button.entry.kind === "container"
            text: qsTr("Ungroup")
            enabled: button.access !== null && button.access.canManage
                     && !button.entry.pending
            onTriggered: button.access.ungroupContainer(
                             button.entry.taskId, button.entry.generationRevision)
        }
        T.MenuSeparator {
            visible: button.dockMode && button.stripMove !== null
        }
        T.MenuItem {
            // Keyboard parity for the drag-reorder gesture (dock mode).
            objectName: "taskListContextMoveLeft"
            visible: button.dockMode && button.stripMove !== null
            text: qsTr("Move left")
            enabled: button.stripMove !== null && button.index > 0
            onTriggered: button.stripMove(button.index, -1)
        }
        T.MenuItem {
            objectName: "taskListContextMoveRight"
            visible: button.dockMode && button.stripMove !== null
            text: qsTr("Move right")
            enabled: button.stripMove !== null && button.access !== null
                     && button.index < button.access.entryCount - 1
            onTriggered: button.stripMove(button.index, 1)
        }
    }
}
