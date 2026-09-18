// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One bounded volume row: label, slider, and known-percent label. The owning
// section supplies the dispatch callback and target naming; availability
// flags come from the route model's shared admission predicate and are never
// widened here.
RowLayout {
    id: root

    required property var targetRow
    required property string kindPrefix
    required property string targetName
    required property var commit
    property alias entryControl: volumeSlider

    spacing: Tokens.space["3"]

    Label {
        text: qsTr("Volume")
        Accessible.name: text
    }

    Slider {
        id: volumeSlider
        objectName: root.kindPrefix + "Volume_" + (root.targetRow?.serial ?? 0)

        readonly property double truth: (root.targetRow?.volumeKnown ?? false)
               ? root.targetRow.volumePercent / 100.0 : 0.0

        Layout.fillWidth: true
        from: 0.0
        to: 1.0
        stepSize: 0.01
        wheelEnabled: true
        enabled: root.targetRow?.volumeAvailable ?? false
        accessibleName: qsTr("%1 volume").arg(root.targetName)
        accessibleDescription: (root.targetRow?.pending ?? false)
            ? qsTr("Volume change in progress")
            : (root.targetRow?.volumeKnown ?? false)
                ? qsTr("%1 percent").arg(root.targetRow.volumePercent)
                : qsTr("Volume unknown")

        // AGENT-CONTRACT (mirrors ADR-0191, shell audio applet): a pressed
        // control owns its value. Truth only rebinds while the handle is not
        // held, so a reprojection mid-drag (which happens on every dispatch,
        // including this row's own) cannot yank the slider out from under
        // the pointer. On release the binding resumes and the next snapshot
        // is authoritative again.
        Binding {
            target: volumeSlider
            property: "value"
            value: volumeSlider.truth
            when: !volumeSlider.pressed
            restoreMode: Binding.RestoreNone
        }

        // Every move dispatches; the model coalesces per target (rejects a
        // second request for the same row while one is in flight rather than
        // disabling the row), so a drag sends the value the pointer is on
        // once the previous request completes. `pressed` must not gate
        // dispatch: Qt reports pressed=true for the whole pointer gesture, so
        // gating on `!pressed` would only ever dispatch a keyboard step.
        onMoved: if (enabled) root.commit(value)
    }

    Label {
        objectName: root.kindPrefix + "VolumeText_" + (root.targetRow?.serial ?? 0)
        text: (root.targetRow?.volumeKnown ?? false)
              ? qsTr("%1%").arg(root.targetRow.volumePercent)
              : qsTr("Unknown")
        muted: true
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
