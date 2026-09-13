// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Demand-attention pulse for the task-list `urgent` projection — the
// Terminal audible-bell path (Qt attention request -> compositor task facts
// -> `urgent`). While `active`, it breathes the host's `urgentAttentionLevel`
// between full and dimmed, and the host binds the opacity of its urgency
// surfaces (panel "!" badge, dock tile icon) to that level.
//
// AGENT-GUARD: opacity-only by contract. Dock magnification owns icon scale,
// and layout bounds and hit targets must never move. Demand-attention truth
// is never animation-dependent: the surfaces stay visible at the dimmest
// point, and the badge is text. The host sets `active` false when urgency
// clears or its reduced-motion policy is on, which stops the loop and settles
// the level back to full opacity.
SequentialAnimation {
    id: root

    property var target: null
    property bool active: false
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
