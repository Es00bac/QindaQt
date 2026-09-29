// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk

// One device's latency offset (ADR-0288): a caption, a whole-millisecond
// field and a reset to 0 ms, bound to the device row's projection.
// AGENT-GUARD: an unknown offset is absent - no field, no reset - never a
// 0 ms reading; `known` gates the whole control.
RowLayout {
    id: root

    required property var targetRow
    required property var audioSettings
    required property string kindPrefix

    readonly property bool known: targetRow?.latencyKnown ?? false
    readonly property bool editable: targetRow?.latencyAvailable ?? false
    // The owning row's focus chain: null when not focusable. Derived from the
    // projection, not from `visible`, which a hidden ancestor would falsify.
    readonly property Item entryControl:
        known && latencyField.enabled ? latencyField.inputItem : null
    readonly property Item resetControl:
        known && latencyReset.enabled ? latencyReset : null

    visible: known
    spacing: Tokens.space["2"]

    Label {
        text: qsTr("Latency offset")
        muted: true
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    // AGENT-CONTRACT (Tk.NumberField, D-198): the model owns `value`; a
    // refused edit snaps back to it. A read-only device widens the bounds to
    // its own value so clamping never displays a number it did not report.
    Tk.NumberField {
        id: latencyField
        objectName: root.kindPrefix + "Latency_" + (root.targetRow?.serial ?? 0)
        small: true
        decimals: 0
        stepSize: 5
        scrub: false
        suffix: qsTr("ms")
        value: root.targetRow?.latencyDisplayMs ?? 0
        from: root.editable ? (root.targetRow?.latencyMinMs ?? 0) : value
        to: root.editable ? (root.targetRow?.latencyMaxMs ?? 0) : value
        enabled: root.editable
        tooltip: qsTr("%1 latency offset in milliseconds")
            .arg(root.targetRow?.displayName ?? "")
        onValueModified: next => root.targetRow !== null
            && root.audioSettings.setDeviceLatencyOffset(
                   root.targetRow.serial, Math.round(next))
    }

    Tk.Button {
        id: latencyReset
        objectName: root.kindPrefix + "LatencyReset_" + (root.targetRow?.serial ?? 0)
        small: true
        text: qsTr("Reset")
        available: root.editable && (root.targetRow?.latencyDisplayMs ?? 0) !== 0
        tooltip: qsTr("Reset %1 latency offset to 0 milliseconds")
            .arg(root.targetRow?.displayName ?? "")
        onClicked: root.targetRow !== null
            && root.audioSettings.setDeviceLatencyOffset(root.targetRow.serial, 0)
    }
}
