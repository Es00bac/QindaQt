// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// One console meter (ADR-0174): an RMS bar with a falling peak marker.
//
// AGENT-CONTRACT: the reading comes from the model's `consoleLevels` channel,
// NOT from the strip or bus row. Rows are republished only when the console's
// configuration changes; levels arrive many times a second on their own
// notification, and binding a meter to a row would leave it frozen.
Rectangle {
    id: meter

    // {peakDb, rmsDb, known} for one console id, or undefined before the first
    // reading. Both levels are dBFS - never the fader scale.
    required property var reading

    // The bottom of the drawn scale. Matches the console's minimum fader gain
    // so the meter and the fader legend describe the same span.
    readonly property real floorDb: -60.0
    readonly property bool known: reading !== undefined && reading.known === true

    function fractionFor(db) {
        if (db === undefined) {
            return 0.0
        }
        return Math.max(0.0, Math.min(1.0, (db - meter.floorDb) / (0.0 - meter.floorDb)))
    }

    readonly property real peakFraction: known ? fractionFor(reading.peakDb) : 0.0
    readonly property real rmsFraction: known ? fractionFor(reading.rmsDb) : 0.0
    // The marker rises instantly and falls slowly, which is what makes a short
    // transient readable at all: at twenty frames a second an instantaneous
    // peak would otherwise be gone before the eye caught it.
    property real heldFraction: 0.0

    onPeakFractionChanged: {
        if (peakFraction > heldFraction) {
            heldFraction = peakFraction
        }
    }

    // AGENT-GUARD: `running` must not read heldFraction. This timer WRITES
    // heldFraction, so gating it on that same value is a binding loop - the
    // property is re-evaluated by the change it caused.
    Timer {
        interval: 100
        repeat: true
        running: meter.visible && meter.known
        onTriggered: {
            if (meter.heldFraction > meter.peakFraction) {
                meter.heldFraction =
                    Math.max(meter.peakFraction, meter.heldFraction - 0.05)
            }
        }
    }

    implicitWidth: 10
    implicitHeight: 160
    radius: 4
    color: Tokens.bg.raised
    Accessible.ignored: true

    Rectangle {
        id: bar
        objectName: "meterBar"
        width: parent.width
        radius: parent.radius
        anchors.bottom: parent.bottom
        visible: meter.known
        height: parent.height * meter.rmsFraction
        // Colour is read off the PEAK, not the bar's own RMS height: what the
        // user needs to see is that the signal is about to clip, and a peak can
        // be at full scale while the RMS bar is still halfway down.
        color: !meter.known || meter.reading.peakDb < -12.0
            ? Tokens.status.success.foreground
            : (meter.reading.peakDb < -3.0
                ? Tokens.status.warning.foreground
                : Tokens.danger.default)
    }

    Rectangle {
        objectName: "meterPeakHold"
        width: parent.width
        height: 2
        visible: meter.known && meter.heldFraction > 0.0
        color: Tokens.fg.default
        y: Math.max(0.0, (parent.height - height) * (1.0 - meter.heldFraction))
    }

    // Unity reference. A console is read against 0 dBFS, so the top of the
    // scale is marked rather than left to be inferred from the track's edge.
    Rectangle {
        width: parent.width
        height: 1
        anchors.top: parent.top
        color: Tokens.outline.divider
    }
}
