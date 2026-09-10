// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Icon-only toolbar button. Themed icons come through the engine's
// "theme-icons" image provider (platform icon theme, ADR-0115); "available"
// maps to the ordinary stock enabled state.
ToolButton {
    id: control
    required property string iconName
    property bool available: true
    enabled: available
    implicitWidth: 40
    implicitHeight: 40
    padding: 8
    flat: true
    display: AbstractButton.IconOnly
    contentItem: Image {
        source: "image://theme-icons/" + control.iconName + "-symbolic"
        sourceSize: Qt.size(20, 20)
        opacity: control.enabled ? 1.0 : 0.45
        Accessible.ignored: true
    }
    ToolTip.visible: hovered || activeFocus
    ToolTip.delay: 600
    ToolTip.text: control.text
    Accessible.name: control.text
}
