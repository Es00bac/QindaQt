// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One titled group of preset cards (built-ins, or the user's own presets)
// that wraps to the page width. Forwards every card action to the page.
ColumnLayout {
    id: section

    required property var customizeSettings
    required property string title
    required property var presets
    property string emptyText: ""
    readonly property Item firstCard: cards.count > 0 && cards.itemAt(0) !== null
                                      ? cards.itemAt(0).selectButton : null

    signal presetAction(string action, var preset)

    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: section.title
    }

    Label {
        Layout.fillWidth: true
        visible: section.presets.length === 0 && section.emptyText.length > 0
        text: section.emptyText
        muted: true
        wrapMode: Text.Wrap
        Accessible.name: text
    }

    Flow {
        objectName: section.objectName.length > 0 ? section.objectName + "Cards" : ""
        Layout.fillWidth: true
        spacing: Tokens.space["3"]
        visible: section.presets.length > 0
        Accessible.role: Accessible.List
        Accessible.name: section.title

        Repeater {
            id: cards

            model: section.presets

            delegate: CustomizeProfileCard {
                required property var modelData

                profile: modelData
                selected: modelData.active === true
                available: section.customizeSettings.canSwitch === true
                actionsAvailable: section.customizeSettings.canManage === true
                onActionRequested: action => section.presetAction(action, modelData)
            }
        }
    }
}
