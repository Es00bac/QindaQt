// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// One console meter (ADR-0174): a segmented LED column with a falling peak
// marker — the classic desk meter. The column is QindaTK's Tk.Meter turned
// vertical, segmented, and coloured by the shared `load` ramp so a hot
// channel reads the same here as everywhere else on the desktop.
//
// AGENT-CONTRACT: the reading comes from the model's `consoleLevels` channel,
// NOT from the strip or bus row. Rows are republished only when the console's
// configuration changes; levels arrive many times a second on their own
// notification, and binding a meter to a row would leave it frozen.
Item {
    id: meter

    // {peakDb, rmsDb, known} for one console id, or undefined before the first
    // reading. Both levels are dBFS - never the fader scale (ADR-0171 keeps
    // the two apart deliberately).
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

    implicitWidth: 14
    implicitHeight: 150
    Accessible.ignored: true

    Tk.Meter {
        objectName: "meterBar"
        anchors.fill: parent
        vertical: true
        from: meter.floorDb
        to: 0.0
        // An unknown reading is an EMPTY meter, never a fabricated one: the
        // console only draws what the service actually published.
        value: meter.known ? meter.reading.rmsDb : meter.floorDb
        segments: 20
        segmentGap: 1
        radius: Tk.Theme.radius.xs
        ramp: Tk.Theme.ramp.load
        opacity: meter.known ? 1.0 : 0.4
    }

    Rectangle {
        objectName: "meterPeakHold"
        width: parent.width - 2
        height: 2
        x: 1
        visible: meter.known && meter.heldFraction > 0.0
        color: meter.reading !== undefined && meter.reading.peakDb > -3.0
            ? Tk.Theme.color.danger : Tk.Theme.color.text
        // fraction 1 is 0 dBFS at the TOP of the column.
        y: 1 + (parent.height - 4) * (1.0 - meter.heldFraction)
    }

    // Unity reference. A console is read against 0 dBFS, so the top of the
    // scale is marked rather than left to be inferred from the well's edge.
    Rectangle {
        width: parent.width
        height: 1
        anchors.top: parent.top
        color: Tk.Theme.color.divider
    }
}
