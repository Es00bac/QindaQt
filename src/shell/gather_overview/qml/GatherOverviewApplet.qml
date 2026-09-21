// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// The panel button that raises the gather overview (ADR-0232).
//
// AGENT-CONTRACT: `access` is the GatherOverviewComposition, the same object
// the Meta+G shortcut and the upper-left screen corner call. The button calls
// `toggle()` and reads `controller.open`; it never decides anything about the
// overview itself, so the three doors cannot disagree about whether it is up.
//
// A null access is a session whose task-list grants were denied. The button
// then draws in the disabled role and says why, rather than vanishing: an
// applet a profile placed and the user cannot see reads as a bug.
//
// AGENT-CONTRACT: every colour, radius and duration is a QST-1 role.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && access !== undefined
                                  && Tokens.ready
    readonly property var controller: root.ready ? root.access.controller : null
    readonly property bool opened: root.controller !== null
                                   && root.controller !== undefined
                                   && Boolean(root.controller.open)
    readonly property bool reducedMotion: Tokens.ready
                                          && Tokens.accessibility.reducedMotion

    objectName: "gatherOverviewApplet"
    implicitWidth: 28
    implicitHeight: 28

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Gather")
    Accessible.description: root.ready
                            ? qsTr("Arranges every open window in one temporary overview")
                            : qsTr("Unavailable: this session has no window list to gather")
    Accessible.onPressAction: if (root.ready) root.access.toggle()

    Rectangle {
        id: surface
        objectName: "gatherOverviewAppletSurface"
        anchors.fill: parent
        radius: Tokens.ready ? Tokens.radius.s : 0
        // AGENT-GUARD: Tokens.state carries exactly `hover` and `pressed`.
        // There is no `state.selected`, and reading one resolves to undefined
        // and paints nothing at all rather than failing loudly. The open state
        // wears accent.subtle, which is the role for a lit affordance.
        color: !Tokens.ready
               ? "transparent"
               : root.opened
                 ? Tokens.accent.subtle
                 : hover.hovered && root.ready
                   ? Tokens.state.hover
                   : "transparent"

        Behavior on color {
            enabled: !root.reducedMotion && Tokens.ready
            ColorAnimation { duration: Tokens.ready ? Tokens.motion.short : 0 }
        }

        ShellIcons.Icon {
            objectName: "gatherOverviewAppletIcon"
            anchors.centerIn: parent
            // The freedesktop name for a window grid; the shell's icon
            // resolver falls back to the text below when a theme lacks it.
            name: "view-grid"
            size: Math.max(1, Math.round(Math.min(root.width, root.height) * 0.62))
            color: !Tokens.ready ? "transparent"
                   : !root.ready ? Tokens.fg.disabled
                   : root.opened ? Tokens.accent.default
                                 : Tokens.fg.default
            fallbackText: qsTr("Gather")
            Accessible.ignored: true
        }
    }

    HoverHandler {
        id: hover
        objectName: "gatherOverviewAppletHover"
        enabled: root.ready
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        objectName: "gatherOverviewAppletTap"
        enabled: root.ready
        onTapped: root.access.toggle()
    }
}
