// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One labelled control in a rack block (ADR-0179): a slider with its value
// read out in the unit the wire uses, committing only when released so a drag
// is one operation rather than a stream of them.
ColumnLayout {
    id: dial

    required property string label
    required property real from
    required property real to
    required property real value
    property string unit: ""
    property int decimals: 0
    property bool enabledControl: true

    signal committed(real value)

    spacing: 0
    Layout.fillWidth: true

    Label {
        Layout.fillWidth: true
        text: dial.label + "  " + Number(slider.pressed ? slider.value : dial.value).toFixed(dial.decimals) + dial.unit
        font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })
        elide: Text.ElideRight
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        from: dial.from
        to: dial.to
        value: dial.value
        enabled: dial.enabledControl
        Accessible.name: dial.label
        onPressedChanged: if (!pressed && value !== dial.value) dial.committed(value)
    }
}
