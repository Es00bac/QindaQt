// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0

// One Settings1 token choice as a form row of segment buttons (ADR-0129):
// the draft value selects a segment and a pick writes the draft. Shared by
// the Windows section and its title-bar options (ADR-0264).
FormRow {
    id: choiceRow

    required property var settings
    required property bool canEdit
    required property string settingsKey
    required property string choiceObjectName
    required property var choices
    property string hint: ""
    readonly property Item firstChoice: segmented.firstChoice

    Layout.fillWidth: true
    description: ""
    editor: segmented
    T.ToolTip.visible: hintHover.hovered && hint.length > 0
    T.ToolTip.delay: 600
    T.ToolTip.text: hint

    HoverHandler { id: hintHover }

    SegmentedChoiceRow {
        id: segmented
        objectName: choiceRow.choiceObjectName
        choices: choiceRow.choices
        currentValue: choiceRow.settings.draft[choiceRow.settingsKey]
                      ?? choiceRow.choices[0].token
        editable: choiceRow.canEdit
        descriptionPrefix: choiceRow.label
        onChoicePicked: token => choiceRow.settings.setDraftValue(
                            choiceRow.settingsKey, token)
    }
}
