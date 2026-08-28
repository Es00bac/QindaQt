// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.Label {
    id: control

    property bool muted: false

    color: enabled ? (muted ? Tokens.fg.muted : Tokens.fg.default)
                   : Tokens.fg.disabled
    font.family: Tokens.type.fontFamily
    font.pointSize: Tokens.type.body
    wrapMode: Text.Wrap
    Accessible.role: Accessible.StaticText
    Accessible.name: text
}
