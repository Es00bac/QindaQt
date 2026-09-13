// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Demand-attention cue for the task-list `urgent` projection — the Terminal
// audible-bell path (Qt attention request -> compositor task facts ->
// `urgent`). One host instantiation per row covers both halves:
//
// - Static truth: `showBadge` renders a "!" text mark whenever the host row
//   is urgent. It is the same text truth as the panel's inline badge, so an
//   urgent dock tile stays visibly distinct from a quiet one with reduced
//   motion enabled and at every pulse rest frame. It never animates.
// - Motion enhancement: while `active`, the animation breathes the host's
//   `urgentAttentionLevel` between full and dimmed, and the host binds its
//   urgency-surface opacities to that level.
//
// AGENT-GUARD: opacity/text only. Dock magnification owns icon scale, and
// layout bounds and hit targets must never move. The host sets `active`
// false when urgency clears or its reduced-motion policy is on, which stops
// the loop and settles the level back to full opacity.
Item {
    id: root

    property var target: null
    property bool active: false
    property bool showBadge: false
    // Fill the host surface so the badge can anchor to its corners; the
    // item itself is inert (no handlers) and never intercepts input.
    anchors.fill: parent

    SequentialAnimation {
        objectName: "taskListUrgentAttentionPulse"
        running: root.active
        loops: Animation.Infinite
        onStopped: if (root.target !== null) root.target.urgentAttentionLevel = 1.0
        NumberAnimation {
            target: root.target
            property: "urgentAttentionLevel"
            from: 1.0
            to: 0.55
            duration: Tokens.motion.short
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: root.target
            property: "urgentAttentionLevel"
            from: 0.55
            to: 1.0
            duration: Tokens.motion.short
            easing.type: Easing.InOutSine
        }
    }

    Text {
        objectName: "taskListEntryDockUrgentBadge"
        visible: root.showBadge
        text: "!"
        color: Tokens.fg.default
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.caption
        font.bold: true
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Tokens.space["1"]
        Accessible.ignored: true
    }
}
