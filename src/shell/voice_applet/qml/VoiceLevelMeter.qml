// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// A segmented capture-level lamp row for the panel chip and the popup header.
//
// AGENT-GUARD: drawn from plain Items, never Canvas. Canvas content does not
// paint under offscreen capture, which is how this surface is reviewed.
//
// AGENT-NOTE: the meter reads a 0-100 percentage the provider already bounded,
// not a dBFS figure. It is deliberately coarser than the audio applet's LED
// meter: this is a "the microphone is hearing you" indicator, not a mixer.
Item {
    id: meter

    // 0-100. Anything outside that range is clamped rather than dropped, so a
    // provider glitch cannot blank the only sign that capture is live.
    required property int levelPercent
    property bool active: true
    property int segments: 5

    readonly property real fraction:
        Math.max(0.0, Math.min(1.0, meter.levelPercent / 100.0))

    implicitWidth: segments * 3 + (segments - 1) * 2
    implicitHeight: 12

    Accessible.ignored: true

    Row {
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: meter.segments

            Rectangle {
                required property int index

                readonly property real threshold: (index + 1) / meter.segments
                readonly property bool lit:
                    meter.active && meter.fraction >= threshold - (0.5 / meter.segments)

                width: 3
                radius: 1.5
                height: meter.implicitHeight * (0.45 + 0.55 * ((index + 1) / meter.segments))
                anchors.verticalCenter: parent.verticalCenter
                color: !lit
                       ? Tokens.outline.divider
                       : threshold > 0.85 ? Tokens.status.warning : Tokens.accent.default

                Behavior on color {
                    ColorAnimation { duration: 90 }
                }
            }
        }
    }
}
