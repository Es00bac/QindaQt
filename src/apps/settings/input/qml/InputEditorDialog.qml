// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Dialog {
    id:root
    modal:true
    focus:true
    padding:Tokens.space["4"]
    palette.window:Tokens.bg.base
    palette.windowText:Tokens.fg.default
    palette.base:Tokens.bg.raised
    palette.text:Tokens.fg.default
    palette.button:Tokens.bg.raised
    palette.buttonText:Tokens.fg.default
    palette.highlight:Tokens.accent.default
    palette.highlightedText:Tokens.accent.fg
    background:Rectangle { color:Tokens.bg.base; radius:Tokens.radius.m; border.width:1; border.color:Tokens.outline.strong }
    header:Label {
        text:root.title
        textFormat:Text.PlainText
        font.pointSize:Tokens.type.title
        font.weight:Font.DemiBold
        padding:Tokens.space["4"]
    palette.window:Tokens.bg.base
    palette.windowText:Tokens.fg.default
    palette.base:Tokens.bg.raised
    palette.text:Tokens.fg.default
    palette.button:Tokens.bg.raised
    palette.buttonText:Tokens.fg.default
    palette.highlight:Tokens.accent.default
    palette.highlightedText:Tokens.accent.fg
        wrapMode:Text.Wrap
        Accessible.role:Accessible.Heading
    }
}
