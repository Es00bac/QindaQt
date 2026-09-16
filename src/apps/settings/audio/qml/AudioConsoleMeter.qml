// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0

// One console meter (ADR-0174): a segmented LED column with a falling peak
// marker — the classic desk meter, drawn from design tokens rather than a
// bitmap theme.
//
// AGENT-CONTRACT: the reading comes from the model's `consoleLevels` channel,
// NOT from the strip or bus row. Rows are republished only when the console's
// configuration changes; levels arrive many times a second on their own
// notification, and binding a meter to a row would leave it frozen.
Item {
    id: meter

    // {peakDb, rmsDb, known} for one console id, or undefined before the first
    // reading. Both levels are dBFS - never the fader scale.
    required property var reading

    // The bottom of the drawn scale. Matches the console's minimum fader gain
    // so the meter and the fader describe the same span.
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

    implicitWidth: 12
    implicitHeight: 150
    Accessible.ignored: true


    // The well behind the segments; the segments themselves are the bar, so
    // the bar keeps the name the probe scripts know it by.
    Rectangle {
        anchors.fill: parent
        radius: Tokens.radius.s
        color: Tokens.bg.base
    }

    ColumnLayout {
        id: meterBar
        objectName: "meterBar"
        anchors.fill: parent
        anchors.margins: 1
        spacing: 1

        Repeater {
            // Bottom segment = floorDb, top segment = 0 dBFS. The count is
            // fixed rather than derived from height so a resized meter keeps
            // the same segment feel; height stretches the segments instead.
            model: 25
            delegate: Rectangle {
                required property int index
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 1
                readonly property real segmentDb:
                    meter.floorDb + (index + 1) * (0.0 - meter.floorDb) / 25
                readonly property bool lit: meter.known && meter.reading.rmsDb >= segmentDb
                // AGENT-NOTE: status.*.background IS the lamp colour;
                // *.foreground is the contrast colour for text sitting on
                // the lamp, which reads as black-on-dark here.
                color: !lit
                    ? Tokens.bg.raised
                    : segmentDb > -3.0 ? Tokens.danger.default
                      : segmentDb > -12.0 ? Tokens.status.warning.background
                        : Tokens.status.success.background
            }
        }
    }

    Rectangle {
        objectName: "meterPeakHold"
        width: parent.width - 2
        height: 2
        x: 1
        visible: meter.known && meter.heldFraction > 0.0
        color: meter.reading !== undefined && meter.reading.peakDb > -3.0
            ? Tokens.danger.default : Tokens.fg.default
        // fraction 1 is 0 dBFS at the TOP of the column.
        y: 1 + (parent.height - 4) * (1.0 - meter.heldFraction)
    }

    // Unity reference. A console is read against 0 dBFS, so the top of the
    // scale is marked rather than left to be inferred from the well's edge.
    Rectangle {
        width: parent.width
        height: 1
        anchors.top: parent.top
        color: Tokens.outline.divider
    }
}
