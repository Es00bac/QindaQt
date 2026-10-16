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
// controller's generationRevision, the revision of the generation the strip is
// showing, so the T0 stale-id arbitration can refuse actions against a
// generation the user no longer sees.
//
// AGENT-CONTRACT (ADR-0390): `entry` carries no revision. The snapshot
// revision advances for every row on every publication, so a retained row
// cannot hold it; it is read from `access` at the moment of the gesture.
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
    // ADR-0265: the dock facade, for "Keep in Dock"; null without a dock.
    property var dockAccess: null
    property bool vertical: false
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    // Only a genuinely inserted delegate fades in. Keyed row updates retain
    // this object, so title/focus/running changes never replay the animation.
    property real revealProgress: 1.0
    opacity: revealProgress
    NumberAnimation {
        id: revealAnimation
        target: button
        property: "revealProgress"
        to: 1.0
        duration: Tokens.motion.short
        easing.type: Easing.OutCubic
    }
    Component.onCompleted: {
        if (dockMode && !reducedMotion) {
            revealProgress = 0.0
            revealAnimation.start()
        }
    }
    onReducedMotionChanged: {
        if (reducedMotion) {
            revealAnimation.stop()
            revealProgress = 1.0
        }
    }
    property bool luna: false
    // Profile-selected tile shape (ADR-0224). "centered" is the Windows-11
    // taskbar's glyph-only tile with an underline indicator; "rail" is the
    // Unity-style square tile with a leading-edge indicator. `luna` stays a
    // separate boolean because it is a whole dressing, not a tile shape, and
    // `dockMode` always wins: a dock tile is already glyph-only and owns its
    // own magnification envelope.
    property string presentation: "standard"
    readonly property bool centeredTile: presentation === "centered" && !dockMode
    readonly property bool railTile: presentation === "rail" && !dockMode
    readonly property bool glyphOnly: centeredTile || railTile
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
    // Demand-attention (terminal bell) pulse level, 1.0 = rest (TaskListUrgentPulse).
    property real urgentAttentionLevel: 1.0
    readonly property int resolvedDockTileSize: Math.max(24, Math.min(64, dockTileSize))
    // Ungrouped rows (`grouping: "never"`) name the one container member they
    // show: Activate and Close address that window, every other action the
    // whole container. Empty on grouped rows.
    readonly property string targetWindowId: String(entry.windowId ?? "")
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
                 : railTile ? 48
                 : centeredTile ? 40
                 : (vertical ? 32 : Math.max(84, Math.min(168, rowLayout.implicitWidth + 12)))
    implicitHeight: dockMode ? resolvedDockTileSize : railTile ? 48 : 28

    // AGENT-GUARD: the controller re-checks capability, generation, and
    // pending fences on every call; this enabled binding is presentation
    // honesty (busy/unavailable controls look disabled), never the gate.
    enabled: access !== null && access.phaseText === "ready" && !entry.pending

    function dispatchActivate() {
        if (targetWindowId.length > 0)
            access.activateTaskWindow(entry.taskId, targetWindowId,
                                      access.generationRevision)
        else
            access.activateTask(entry.taskId, access.generationRevision)
    }

    function activate() {
        if (enabled)
            dispatchActivate()
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
    Keys.onMenuPressed: openContextMenu()

    Accessible.role: Accessible.Button
    Accessible.name: entry.accessibleName
    Accessible.description: entry.pending
        ? qsTr("Operation pending for this window")
        : qsTr("Press to activate. Press the Menu key for window actions.")

    // AGENT-CONTRACT (ADR-0396): a Popup.Window keeps its own window, and with it
    // a GL context and video memory, for as long as the Popup object exists,
    // even after it closes, so a tooltip and a menu per tile cost a window for
    // every tile ever hovered or right-clicked. Both are therefore created
    // when wanted and destroyed once closed: an idle strip owns no popup
    // window. They name `parent: button` explicitly because a Loader would
    // otherwise parent them to itself, and the tooltip positions against it.
    //
    // The hover preview card replaces the tooltip when the preview seam is
    // present; an unavailable capture degrades to the preview card's
    // title-only form, never to a duplicate floating label.
    readonly property bool tooltipWanted: dockMode && hovered
        && !(access !== null && access.previewsEnabled)
    // Held true from the first time the tooltip is wanted until it has closed
    // and is no longer wanted. It is state, not a binding on the loaded item:
    // `active` reading `item` would be a binding loop, since loading changes it.
    property bool tooltipLoaded: false
    onTooltipWantedChanged: {
        if (tooltipWanted)
            tooltipLoaded = true
        else if (!(tooltipLoader.item as T.ToolTip)?.visible)
            tooltipRelease.restart() // never opened, or already closed
    }
    Timer {
        id: tooltipRelease
        interval: 0
        onTriggered: if (!button.tooltipWanted) button.tooltipLoaded = false
    }
    Loader {
        id: tooltipLoader
        active: button.tooltipLoaded
        sourceComponent: T.ToolTip {
            objectName: "taskListEntryTooltip"
            parent: button
            // Bound once complete, not as an initial value: a popup created
            // visible opens at once and skips `delay`, and the tooltip must
            // keep its hover delay.
            Component.onCompleted: visible = Qt.binding(() => button.tooltipWanted)
            text: button.entry.accessibleName
            delay: Tokens.motion.short
            popupType: T.Popup.Window
            // The exit transition finished, so the window can go.
            onClosed: tooltipRelease.restart()
        }
    }

    // Right-click opens the same context actions the Menu key exposes.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: button.openContextMenu()
    }

    C.TouchContextArea { objectName: "taskListEntryTouchContext"; anchors.fill: parent; onContextRequested: button.openContextMenu() }

    contentItem: Item {
        RowLayout {
            id: rowLayout
            anchors.fill: parent
            visible: !button.dockMode && !button.glyphOnly
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

            // Demand-attention truth is text, never color-only; the urgent
            // breath is opacity on this always-visible-while-urgent badge.
            Text {
                objectName: "taskListEntryUrgentBadge"
                visible: button.entry.urgent
                opacity: button.urgentAttentionLevel
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
            size: Math.min(40, Math.max(
                      16, button.resolvedDockTileSize - 8))
            color: button.resolvedIconColor
            symbolic: button.entry.kind === "container"
            fallbackText: button.entry.applicationName
            opacity: button.urgentAttentionLevel
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

        // The glyph for a tile that carries no title. Deliberately not the
        // dock icon: that one owns the dock's magnification envelope and its
        // bottom-anchored swell, which a flat taskbar tile must not inherit.
        ShellIcons.Icon {
            objectName: "taskListGlyphEntryIcon"
            visible: button.glyphOnly
            anchors.centerIn: parent
            name: String(button.entry.iconName ?? "")
            size: button.railTile ? 26 : 20
            color: button.resolvedIconColor
            symbolic: button.entry.kind === "container"
            fallbackText: button.entry.applicationName
            opacity: button.urgentAttentionLevel
            Accessible.ignored: true
        }

        // A bar, not a dot: the Windows-11 taskbar underlines a running tile
        // on the panel-facing edge and the Unity rail marks its leading edge.
        // It grows while the task is active, so running and focused are two
        // distinguishable states without relying on color alone.
        Rectangle {
            objectName: "taskListPresentationIndicator"
            visible: button.glyphOnly
            width: button.railTile ? 3 : (button.entry.active ? 18 : 6)
            height: button.railTile ? (button.entry.active ? 18 : 6) : 3
            radius: Math.min(width, height) / 2
            color: button.entry.active ? Tokens.accent.default : Tokens.fg.muted
            anchors.horizontalCenter: button.railTile
                                      ? undefined : parent.horizontalCenter
            anchors.bottom: button.railTile ? undefined : parent.bottom
            anchors.bottomMargin: button.railTile ? 0 : 1
            anchors.verticalCenter: button.railTile
                                    ? parent.verticalCenter : undefined
            anchors.left: button.railTile ? parent.left : undefined
            Accessible.ignored: true
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

        TaskListUrgentPulse { target: button; active: entry.urgent && !reducedMotion; showBadge: dockMode && entry.urgent }
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
            opacity: (button.entry.minimized || button.entry.iconified === true) ? 0.7 : 1.0

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
            opacity: (button.entry.minimized || button.entry.iconified === true) ? 0.7 : 1.0
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

    // The menu is created on demand and destroyed after it closes (ADR-0396).
    function openContextMenu() {
        contextMenuLoader.active = true
        const menu = contextMenuLoader.item as T.Menu
        if (menu !== null)
            menu.popup()
    }
    Loader {
        id: contextMenuLoader
        active: false
        sourceComponent: contextMenuComponent
    }
    Timer {
        id: contextMenuRelease
        interval: 0
        onTriggered: contextMenuLoader.active = false
    }

    // QindaQt.Controls ships no menu primitive yet; see TaskListEntryMenu.qml.
    Component {
        id: contextMenuComponent
        TaskListEntryMenu {
            tile: button
            // Closed means the popup window can go (ADR-0396): unload on the next
            // turn, never from inside the signal that announces the close.
            onClosed: contextMenuRelease.start()
        }
    }
}
