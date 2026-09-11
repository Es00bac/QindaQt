// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

// Tokenized hover/focus explanation. Consumers keep the visible surface
// glyph-first and move the sentence here; the tip never carries state a
// control cannot also expose through Accessible.description.
T.ToolTip {
    id: control

    delay: 600
    timeout: 8000
    leftPadding: Tokens.space["3"]
    rightPadding: Tokens.space["3"]
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["2"]
    margins: Tokens.space["2"]

    contentItem: Text {
        text: control.text
        color: Tokens.fg.default
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.caption
        wrapMode: Text.Wrap
        Accessible.ignored: true
    }

    background: Rectangle {
        radius: Tokens.radius.s
        color: Tokens.bg.highest
        border.width: 1
        border.color: Tokens.outline.strong
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Tokens.motion.short }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: Tokens.motion.short }
    }
}
