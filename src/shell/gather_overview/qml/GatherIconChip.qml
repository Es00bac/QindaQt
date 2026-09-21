// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// One iconified window (ADR-0203) as a round chip. The icon lane's whole job
// is to say "this window is rolled up and here it is", so the chip carries
// identity and nothing else - no title, no close affordance.
//
// `item.iconName` is supplied by the controller through its injected icon-name
// resolver, the same way the task list's entries get theirs; the pure model
// carries only applicationId, and resolving a theme icon is not its business.
//
// AGENT-CONTRACT: every colour, radius and duration is a QST-1 role. A fenced
// tile is drawn in the disabled foreground role rather than faded with an
// opacity number, so the theme decides how "unavailable" looks.
Item {
    id: chip

    required property var item
    property bool interactive: true
    property bool reducedMotion: false

    signal activated()

    readonly property bool hovered: hover.hovered && chip.interactive
    readonly property real extent: Math.min(width, height)
    // The glyph sits inside the disc's own edge, which is the chip's frame.
    // A proportion, not a size: the planner owns the chip extent.
    readonly property real glyphProportion: 0.58

    objectName: "gatherIconChip"

    Accessible.role: Accessible.Button
    Accessible.name: String(chip.item.accessibleName
                            || chip.item.title
                            || chip.item.applicationName || "")
    Accessible.description: chip.interactive
                            ? qsTr("Rolled up window. Activating it brings it back.")
                            : qsTr("Rolled up window. Unavailable: the window list is degraded.")
    Accessible.onPressAction: if (chip.interactive) chip.activated()

    Rectangle {
        objectName: "gatherIconChipDisc"
        anchors.centerIn: parent
        width: chip.extent
        height: chip.extent
        radius: width / 2
        color: Tokens.ready
               ? (chip.hovered ? Tokens.state.hover : Tokens.bg.raised)
               : "transparent"
        border.width: 1
        border.color: !Tokens.ready
                      ? "transparent"
                      : !chip.interactive
                        ? Tokens.fg.disabled
                        : chip.item.urgent
                          ? Tokens.danger.default
                          : chip.item.active
                            ? Tokens.accent.default
                            : Tokens.accessibility.highContrast
                              ? Tokens.outline.strong
                              : Tokens.outline.divider

        Behavior on color {
            enabled: !chip.reducedMotion && Tokens.ready
            ColorAnimation { duration: Tokens.ready ? Tokens.motion.short : 0 }
        }

        ShellIcons.Icon {
            objectName: "gatherIconChipIcon"
            anchors.centerIn: parent
            name: String(chip.item.iconName ?? "")
            size: Math.max(1, Math.round(chip.extent * chip.glyphProportion))
            color: !Tokens.ready ? "transparent"
                   : chip.interactive ? Tokens.fg.default : Tokens.fg.disabled
            fallbackText: String(chip.item.iconText
                                 || chip.item.applicationName || "")
            Accessible.ignored: true
        }
    }

    HoverHandler {
        id: hover
        objectName: "gatherIconChipHover"
        enabled: chip.interactive
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        objectName: "gatherIconChipTap"
        enabled: chip.interactive
        onTapped: chip.activated()
    }
}
