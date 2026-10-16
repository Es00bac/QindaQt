// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0

// A horizontal LED meter for the tray console: a segmented lamp row with a
// falling peak marker, the same reading a desk meter gives.
//
// AGENT-NOTE: this is the deliberate twin of
// src/apps/settings/audio/qml/AudioConsoleMeter.qml (vertical, ADR-0174). The
// two live in different QML modules that cannot import each other, and a
// shared control would have to go in src/controls, which would invalidate
// every reviewed gallery baseline. Segment thresholds and colours are kept
// identical on purpose; change them in both files or in neither.
//
// AGENT-GUARD: drawn from plain Items, never Canvas. Canvas content does not
// paint under offscreen capture, which is exactly how this surface is
// reviewed.
Item {
    id: meter

    // {peakDb, rmsDb, known} for one console id, or undefined before the first
    // reading. Both levels are dBFS - never the fader scale.
    required property var reading

    // AGENT-CONTRACT (ADR-0394): the meter stream (Audio1 `Levels`, twenty
    // frames a second) is subscribed only while a meter is on screen. A meter
    // holds one lease on its `levelsOwner` for exactly as long as it is visible
    // in a window that can show it, and gives it back when it is hidden, when
    // its window is hidden or minimized, when the owner changes, and when it is
    // destroyed. The owner counts leases and subscribes on the first and
    // unsubscribes on the last; this file never talks to a client or a bus.
    // An owner without retainMeters() (a test double) simply holds no lease.
    property var levelsOwner: null
    readonly property bool onScreen: meter.visible && Window.window !== null
        && Window.window.visible && Window.window.visibility !== Window.Minimized
    property var leasedOwner: null

    function syncLease() {
        const owner = meter.levelsOwner
        const wanted = meter.onScreen && owner !== null && owner !== undefined
            && typeof owner.retainMeters === "function"
        if (meter.leasedOwner !== null && (!wanted || meter.leasedOwner !== owner)) {
            const leased = meter.leasedOwner
            meter.leasedOwner = null
            leased.releaseMeters()
        }
        if (wanted && meter.leasedOwner === null) {
            meter.leasedOwner = owner
            owner.retainMeters()
        }
    }

    onOnScreenChanged: syncLease()
    onLevelsOwnerChanged: syncLease()
    Component.onCompleted: syncLease()
    Component.onDestruction: {
        if (meter.leasedOwner !== null) {
            const leased = meter.leasedOwner
            meter.leasedOwner = null
            leased.releaseMeters()
        }
    }

    readonly property real floorDb: -60.0
    readonly property bool known: reading !== undefined && reading.known === true

    function fractionFor(db) {
        if (db === undefined) {
            return 0.0
        }
        return Math.max(0.0, Math.min(1.0, (db - meter.floorDb) / (0.0 - meter.floorDb)))
    }

    readonly property real peakFraction: known ? fractionFor(reading.peakDb) : 0.0
    // The marker rises instantly and falls slowly, which is what makes a short
    // transient readable at all.
    property real heldFraction: 0.0

    onPeakFractionChanged: {
        if (peakFraction > heldFraction) {
            heldFraction = peakFraction
        }
    }

    // AGENT-GUARD: `running` must not read heldFraction. This timer WRITES
    // heldFraction, so gating it on that same value is a binding loop.
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

    implicitWidth: 96
    implicitHeight: 8
    Accessible.ignored: true

    Rectangle {
        anchors.fill: parent
        radius: Tokens.radius.s
        color: Tokens.bg.base
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 1

        Repeater {
            // Leftmost segment = floorDb, rightmost = 0 dBFS. A fixed count so
            // a resized meter keeps the same segment feel.
            model: 16
            delegate: Rectangle {
                required property int index
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 1
                readonly property real segmentDb:
                    meter.floorDb + (index + 1) * (0.0 - meter.floorDb) / 16
                readonly property bool lit:
                    meter.known && meter.reading.rmsDb >= segmentDb
                // AGENT-NOTE: status.*.background IS the lamp colour;
                // *.foreground is the contrast colour for text sitting on the
                // lamp, which reads as black-on-dark here.
                color: !lit
                    ? Tokens.bg.raised
                    : segmentDb > -3.0 ? Tokens.danger.default
                      : segmentDb > -12.0 ? Tokens.status.warning.background
                        : Tokens.status.success.background
            }
        }
    }

    Rectangle {
        objectName: "audioMeterPeakHold"
        width: 2
        height: parent.height - 2
        y: 1
        visible: meter.known && meter.heldFraction > 0.0
        color: meter.reading !== undefined && meter.reading.peakDb > -3.0
            ? Tokens.danger.default : Tokens.fg.default
        // fraction 1 is 0 dBFS at the RIGHT edge of the row.
        x: 1 + (parent.width - 4) * meter.heldFraction
    }
}
