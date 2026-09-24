// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0

// A Settings1 token choice with too many options for a segment row (the
// named button styles, ADR-0264): the same draft wiring as ChromeChoice,
// presented as a menu.
FormRow {
    id: menuRow

    required property var settings
    required property bool canEdit
    required property string settingsKey
    required property string choiceObjectName
    required property var choices
    property string hint: ""
    readonly property Item firstChoice: menu

    Layout.fillWidth: true
    description: ""
    editor: menu
    T.ToolTip.visible: menuHover.hovered && hint.length > 0
    T.ToolTip.delay: 600
    T.ToolTip.text: hint

    HoverHandler { id: menuHover }

    ComboBox {
        id: menu
        objectName: menuRow.choiceObjectName
        implicitWidth: 220
        model: menuRow.choices
        textRole: "label"
        valueRole: "token"
        enabled: menuRow.canEdit
        accessibleDescription: menuRow.hint
        // The draft token selects the entry; an unknown token shows the
        // first (default) entry rather than a blank menu.
        currentIndex: {
            const token = menuRow.settings.draft[menuRow.settingsKey]
                          ?? menuRow.choices[0].token
            for (let index = 0; index < menuRow.choices.length; ++index) {
                if (menuRow.choices[index].token === token)
                    return index
            }
            return 0
        }
        onActivated: index => menuRow.settings.setDraftValue(
                         menuRow.settingsKey, menuRow.choices[index].token)
    }
}
