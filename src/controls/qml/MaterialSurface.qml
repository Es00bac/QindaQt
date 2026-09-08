// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

// Local glass layering only: content behind the app never changes text contrast.
// Reduced transparency/high contrast remove decoration rather than fading text.
Rectangle {
    id: surface
    property bool raised: true
    color: raised ? Tokens.bg.raised : Tokens.bg.base
    radius: Tokens.radius.l
    border.width: 1
    border.color: Tokens.accessibility.highContrast ? Tokens.outline.strong : Tokens.outline.divider
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, surface.radius - 1)
        visible: !Tokens.accessibility.reducedTransparency && !Tokens.accessibility.highContrast
        gradient: Gradient {
            GradientStop { position: 0; color: Tokens.state.hover }
            GradientStop { position: 0.48; color: "transparent" }
            GradientStop { position: 1; color: Tokens.accent.subtle }
        }
        Accessible.ignored: true
    }
}
