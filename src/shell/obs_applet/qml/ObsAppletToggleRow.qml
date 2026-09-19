// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One of the popup's three toggles: what it is, how long it has been doing
// it, and a compact switch. Restore the authoritative binding after a press:
// the switch may not keep a locally toggled state when OBS refuses it.
RowLayout {
    id: root

    property string label: ""
    // A dash until OBS reports a duration; an empty string hides the readout
    // entirely for an output that has none.
    property string elapsed: ""
    property bool active: false
    property bool available: false
    property string accessibleDescription: ""

    signal toggled()

    spacing: Tokens.space["3"]

    Label {
        Layout.fillWidth: true
        text: root.label
    }

    Label {
        objectName: "obsToggleElapsed"
        visible: root.elapsed.length > 0 && root.active
        text: root.elapsed
        muted: true
        font.family: Tokens.type.monoFontFamily
    }

    Switch {
        objectName: "obsToggleSwitch"
        enabled: root.available
        checked: root.active
        accessibleDescription: root.accessibleDescription
        Accessible.name: root.label
        onToggled: {
            root.toggled()
            checked = Qt.binding(function() { return root.active })
        }
    }
}
