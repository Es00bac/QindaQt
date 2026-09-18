// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One labelled closed choice over a {token, label} list. The row owns no
// domain knowledge: the page supplies the choices, the current draft token,
// and the help text, and receives one choicePicked(token) per explicit user
// activation (never for a confirmed refresh moving currentIndex).
GridLayout {
    id: row

    required property string label
    required property var choices
    required property string currentToken
    required property bool editable
    required property string help
    required property string selectorObjectName
    property bool compact: false
    signal choicePicked(string token)
    readonly property Item selector: choiceSelector

    function choiceIndex(token) {
        for (let index = 0; index < choices.length; ++index) {
            if (choices[index].token === token) {
                return index
            }
        }
        return -1
    }

    Layout.fillWidth: true
    columns: compact ? 1 : 2
    columnSpacing: Tokens.space["3"]
    rowSpacing: Tokens.space["2"]

    Label {
        text: row.label
        Accessible.ignored: true
    }

    ComboBox {
        id: choiceSelector
        objectName: row.selectorObjectName
        Layout.fillWidth: true
        enabled: row.editable
        model: row.choices
        textRole: "label"
        valueRole: "token"
        currentIndex: row.choiceIndex(row.currentToken)
        accessibleDescription: row.help
        T.ToolTip.text: row.help
        T.ToolTip.visible: hovered
        T.ToolTip.delay: 500
        onActivated: index => row.choicePicked(row.choices[index].token)
    }
}
