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
            description: qsTr("Choose when this panel stays out of the way")
            editor: visibilityChoices
            Flow {
                id: visibilityChoices
                spacing: Tokens.space["1"]
                Repeater {
                    model: [
                        { token: "never", label: qsTr("Always visible") },
                        { token: "intelligent", label: qsTr("Auto-hide") },
                        { token: "dodge-active", label: qsTr("Dodge active window") },
                        { token: "dodge-all", label: qsTr("Dodge all windows") },
                        { token: "maximized", label: qsTr("Hide when maximized") }
                    ]
                    delegate: Button {
                        required property var modelData
                        text: modelData.label
                        checkable: true
                        autoExclusive: true
                        checked: root.properties.hideMode === modelData.token
                        available: root.customizeSettings.canEdit
                        emphasized: checked
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: checked
                        onClicked: root.customizeSettings.configureSelectedPanel("hideMode", modelData.token)
                    }
                }
            }
        }
    }
}
