// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.Pane {
    id: control

    padding: Tokens.space["5"]
    Accessible.role: Accessible.Grouping

    background: Rectangle {
        color: Tokens.bg.raised
        radius: Tokens.radius.l
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.divider
    }
}
