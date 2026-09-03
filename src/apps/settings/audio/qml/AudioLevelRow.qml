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
        objectName: root.kindPrefix + "Volume_" + root.targetRow.serial
        Layout.fillWidth: true
        from: 0.0
        to: 1.0
        stepSize: 0.01
        // The value follows projected truth through the binding; the owning
        // delegate row is recreated whenever the projection changes, so a
        // broken-by-drag binding cannot outlive one refresh.
        value: root.targetRow.volumeKnown
               ? root.targetRow.volumePercent / 100.0 : 0.0
        enabled: root.targetRow.volumeAvailable
        accessibleName: qsTr("%1 volume").arg(root.targetName)
        accessibleDescription: root.targetRow.volumeKnown
            ? qsTr("%1 percent").arg(root.targetRow.volumePercent)
            : qsTr("Volume unknown")
        // Dispatch on release or on each keyboard step. Keyboard steps
        // arrive with pressed already false, so the keyboard path requests
        // immediately while a pointer drag stays quiet until it ends.
        onMoved: if (!pressed && enabled) root.commit(value)
    }

    Label {
        objectName: root.kindPrefix + "VolumeText_" + root.targetRow.serial
        text: root.targetRow.volumeKnown
              ? qsTr("%1%").arg(root.targetRow.volumePercent)
              : qsTr("Unknown")
        muted: true
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
