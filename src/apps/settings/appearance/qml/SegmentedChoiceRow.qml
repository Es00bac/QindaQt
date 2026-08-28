// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One mutually exclusive string-token choice presented as a button row with
// radio semantics. The component owns no domain knowledge: callers supply
// choices, the current draft token, and an accessibility prefix, and receive
// one choicePicked(token) per explicit user selection.
Row {
    id: root

    required property var choices
    required property string currentValue
    required property bool editable
    required property string descriptionPrefix
    signal choicePicked(string token)

    spacing: Tokens.space["2"]

    Repeater {
        model: root.choices

        delegate: Button {
            id: choiceButton

            required property var modelData

            objectName: root.objectName + "_" + choiceButton.modelData.token
            checkable: true
            autoExclusive: true
            available: root.editable
            text: choiceButton.modelData.label
            checked: root.currentValue === choiceButton.modelData.token
            Accessible.role: Accessible.RadioButton
            Accessible.checked: checked
            Accessible.description: root.descriptionPrefix + " "
                                    + choiceButton.modelData.label
            onToggled: checked => {
                if (checked) {
                    root.choicePicked(choiceButton.modelData.token)
                }
            }
        }
    }
}
