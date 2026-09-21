// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import QindaTK as Tk

// A console desk pad (M, S, Mono, A1, B2, Rack, Record…): the smallest
// clickable unit the mixing console has, lit like a desk lamp when engaged.
//
// AGENT-NOTE: belongs in QindaTK as a LampButton/Pad next to Button. The
// toolkit's smallest Button is 20px with form padding, and nothing in the
// toolkit has lamp semantics (an engaged key glowing in a role colour —
// accent for routing, warning for solo, danger for mute/record). Built
// locally on QtQuick.Templates, the same way QindaTK builds its own controls,
// because the QindaTK repository must not be changed from this lane.
T.Button {
    id: pad

    property bool available: true
    property bool busy: false
    property bool destructive: false
    // The lit colours. Voicemeeter parity: an engaged routing key glows in
    // the accent colour; mute and record lamps read as danger instead.
    property color lampColor: Tk.Theme.color.accent
    property color lampTextColor: Tk.Theme.color.accentContrast

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    enabled: available && !busy
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: 18
    leftPadding: Tk.Theme.space.xs
    rightPadding: Tk.Theme.space.xs
    topPadding: 1
    bottomPadding: 1
    opacity: enabled ? 1.0 : Tk.Theme.opacity.disabled
    font.pixelSize: Tk.Theme.font.caption

    Accessible.role: Accessible.Button
    Accessible.name: text

    background: Rectangle {
        radius: Tk.Theme.radius.xs
        color: !pad.enabled ? Tk.Theme.color.controlBg
             : pad.checked ? pad.lampColor
             : pad.down ? Tk.Theme.color.pressed
             : pad.hovered ? Tk.Theme.color.controlHoverBg
             : Tk.Theme.color.controlBg
        border.width: Tk.Theme.size.border
        border.color: pad.destructive && !pad.checked ? Tk.Theme.color.danger
             : pad.checked ? pad.lampColor
             : pad.hovered ? Tk.Theme.color.controlHoverBorder
             : Tk.Theme.color.controlBorder

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            radius: parent.radius + 1
            color: "transparent"
            border.width: Tk.Theme.size.focusRing
            border.color: Tk.Theme.color.focus
            visible: pad.visualFocus
        }
    }

    contentItem: Text {
        text: pad.text
        color: !pad.enabled ? Tk.Theme.color.textDisabled
             : pad.checked ? pad.lampTextColor : Tk.Theme.color.text
        font: pad.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
