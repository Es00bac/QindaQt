// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import "DisplayArrangementGeometry.js" as Geometry

// Section for selecting the UI scale of the selected display, with guidance
// on what each preset means for this display's resolution.
ColumnLayout {
    id: root

    required property var displaySettings
    required property bool editorBusy

    readonly property double currentScale: root.displaySettings.selectedOutput.scale ?? 1.0
    readonly property bool outputEnabled: root.displaySettings.selectedOutput.enabled ?? false
    readonly property var currentMode: Geometry.modeFor(root.displaySettings.selectedOutput)
    readonly property var currentLogical: Geometry.logicalSizeForScale(
                                              root.displaySettings.selectedOutput, root.currentScale)
    readonly property double typicalScale: Geometry.typicalScaleFor(root.currentMode)
    readonly property string currentSummary: {
        const percent = Geometry.formatPercent(root.currentScale)
        if (root.currentLogical === null) {
            return qsTr("Current scale: %1").arg(percent)
        }
        return root.currentLogical.integral
                ? qsTr("Current scale: %1 → %2 × %3 logical, whole pixels")
                  .arg(percent).arg(root.currentLogical.width).arg(root.currentLogical.height)
                : qsTr("Current scale: %1 → %2 × %3 logical, fractional (text may look softer)")
                  .arg(percent).arg(root.currentLogical.width).arg(root.currentLogical.height)
    }
    readonly property string guidanceText: {
        if (root.currentMode === null) {
            return qsTr("Larger percentages make text and controls bigger and leave less room on this display.")
        }
        return qsTr("Typical for %1 × %2: %3. Larger percentages make text and controls bigger and leave less room; neighbouring displays re-attach automatically when this display's size changes.")
                .arg(root.currentMode.pixelWidth).arg(root.currentMode.pixelHeight)
                .arg(Geometry.formatPercent(root.typicalScale))
    }

    readonly property var scalePresets: [
        { label: "100%", value: 1.0 },
        { label: "125%", value: 1.25 },
        { label: "150%", value: 1.5 },
        { label: "175%", value: 1.75 },
        { label: "200%", value: 2.0 },
        { label: "250%", value: 2.5 },
        { label: "300%", value: 3.0 }
    ]

    spacing: Tokens.space["3"]

    function presetDescription(value) {
        const logical = Geometry.logicalSizeForScale(root.displaySettings.selectedOutput, value)
        const typical = Math.abs(value - root.typicalScale) < 0.01 ? qsTr(" Typical for this resolution.") : ""
        if (logical === null) {
            return qsTr("Scale factor %1.").arg(Geometry.formatPercent(value)) + typical
        }
        return (logical.integral
                ? qsTr("%1 × %2 logical, whole pixels.").arg(logical.width).arg(logical.height)
                : qsTr("%1 × %2 logical, fractional; text may look softer.")
                  .arg(logical.width).arg(logical.height)) + typical
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Scale")
        description: root.currentSummary
        editor: scaleChoiceRow

        GridLayout {
            id: scaleChoiceRow
            objectName: "displayScaleChoiceRow"
            // Four choices per row keep every preset visible in the 960px
            // window and compact layout; a long, clipped selector hides the
            // 300% option and leaves keyboard users without a reachable value.
            columns: 4
            columnSpacing: Tokens.space["2"]
            rowSpacing: Tokens.space["2"]

            Repeater {
                model: root.scalePresets

                delegate: Button {
                    id: scaleBtn
                    required property var modelData

                    objectName: "displayScaleButton_" + Math.round(scaleBtn.modelData.value * 100)
                    checkable: true
                    autoExclusive: true
                    Layout.fillWidth: true
                    available: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                    text: scaleBtn.modelData.label
                    checked: Math.abs(root.currentScale - scaleBtn.modelData.value) < 0.01
                    // Only the chosen preset carries the amber fill so the
                    // current value reads at a glance.
                    emphasized: checked
                    accessibleDescription: root.presetDescription(scaleBtn.modelData.value)

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: qsTr("Scale %1").arg(scaleBtn.modelData.label)
                    Accessible.checked: checked

                    onClicked: {
                        if (root.displaySettings.selectedOutputId) {
                            root.displaySettings.setOutputScale(
                                root.displaySettings.selectedOutputId, scaleBtn.modelData.value)
                        }
                    }
                }
            }
        }
    }

    Label {
        objectName: "displayScaleGuidance"
        Layout.fillWidth: true
        text: root.guidanceText
        font.pointSize: Tokens.type.caption
        muted: true
        textFormat: Text.PlainText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
