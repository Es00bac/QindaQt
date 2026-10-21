// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0
Item {
    id:root
    default property alias buttons:actions.data
    implicitHeight:actions.implicitHeight + 2 * Tokens.space["3"]
    RowLayout {
        id:actions
        anchors.fill:parent
        anchors.margins:Tokens.space["3"]
        spacing:Tokens.space["2"]
    }
}
