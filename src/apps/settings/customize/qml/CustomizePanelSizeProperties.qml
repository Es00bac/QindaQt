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
            label: qsTr("Thickness")
            description: qsTr("%1 logical pixels").arg(Math.round(thicknessSlider.value))
            editor: thicknessSlider
            Slider {
                id: thicknessSlider
                objectName: "customizeThicknessSlider"
                from: 20
                to: 192
                stepSize: 1
                value: root.properties.thickness ?? 32
                enabled: root.customizeSettings.canEdit
                accessibleName: qsTr("Panel thickness")
                onMoved: root.customizeSettings.configureSelectedPanel("thickness", Math.round(value))
            }
        }
        FormRow {
            Layout.fillWidth: true
            label: qsTr("Length")
            description: qsTr("%1 percent").arg(Math.round(lengthSlider.value * 100))
            editor: lengthSlider
            Slider {
                id: lengthSlider
                objectName: "customizeLengthSlider"
                from: 0.1
                to: 1.0
                stepSize: 0.05
                value: root.properties.length ?? 1.0
                enabled: root.customizeSettings.canEdit
                accessibleName: qsTr("Panel length")
                onMoved: root.customizeSettings.configureSelectedPanel("length", value)
            }
        }
    }
}
