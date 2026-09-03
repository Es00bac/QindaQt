// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

FormSurface {
    id: root
    required property var customizeSettings
    required property var properties

    ColumnLayout {
        width: parent.width
        spacing: Tokens.space["2"]
        FormRow {
            Layout.fillWidth: true
            label: qsTr("Visibility")
            description: qsTr("Always-hidden is unavailable until reveal controls land")
            editor: visibilityChoices
            Flow {
                id: visibilityChoices
                spacing: Tokens.space["1"]
                Repeater {
                    model: ["never", "intelligent", "dodge-active", "dodge-all", "maximized"]
                    delegate: Button {
                        required property string modelData
                        text: modelData
                        checkable: true
                        autoExclusive: true
                        checked: root.properties.hideMode === modelData
                        available: root.customizeSettings.canEdit
                        emphasized: checked
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: checked
                        onClicked: root.customizeSettings.configureSelectedPanel("hideMode", modelData)
                    }
                }
            }
        }
    }
}
