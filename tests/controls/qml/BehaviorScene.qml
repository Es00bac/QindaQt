// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

Item {
    id: scene

    property bool rtl: false
    property string longLabel: qsTr("A deliberately extended localized label that must wrap without hiding its editor");
    property alias primaryButton: primary
    property alias busyButton: busy
    property alias disabledButton: disabled
    property alias checkBox: check
    property alias toggleSwitch: toggle
    property alias slider: level
    property alias textField: field
    property alias formRow: row
    property alias stateCard: stateCard
    property alias busyStateCard: busyStateCard
    property alias degradedNotice: degraded
    property alias themeCard: themeCard
    property alias tokenSwatch: swatch
    property alias specimenLabel: specimenLabel

    width: 720
    height: 1080
    LayoutMirroring.enabled: rtl
    LayoutMirroring.childrenInherit: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        C.SectionHeader {
            objectName: "sectionHeader"
            Layout.fillWidth: true
            title: qsTr("Accessible controls")
            description: qsTr("Every action is reachable with a keyboard and exposes its state.")
        }

        RowLayout {
            Layout.fillWidth: true

            C.Button {
                id: primary
                objectName: "primaryButton"
                text: qsTr("Apply")
                accessibleDescription: qsTr("Apply the pending example values")
            }

            C.Button {
                id: busy
                objectName: "busyButton"
                text: qsTr("Save")
                busy: true
                accessibleDescription: qsTr("Save the example values")
            }

            C.Button {
                id: disabled
                objectName: "disabledButton"
                text: qsTr("Unavailable action")
                available: false
                error: true
                accessibleDescription: qsTr("Correct the example value first")
            }
        }

        C.FormSurface {
            objectName: "formSurface"
            Layout.fillWidth: true

            ColumnLayout {
                width: parent.width

                C.FormRow {
                    id: row
                    objectName: "formRow"
                    Layout.fillWidth: true
                    label: scene.longLabel
                    description: qsTr("This helper text also grows under localization.")
                    required: true
                    errorMessage: qsTr("Enter a valid value before continuing.")
                    editor: field

                    C.TextField {
                        id: field
                        objectName: "textField"
                        width: parent.width
                        placeholderText: qsTr("Example value")
                        accessibleName: qsTr("Standalone editor name")
                        accessibleDescription: qsTr("Standalone editor description")
                        error: row.errorMessage.length > 0
                    }
                }

                C.CheckBox {
                    id: check
                    objectName: "checkBox"
                    Layout.fillWidth: true
                    text: qsTr("Enable the optional feature")
                }

                C.Switch {
                    id: toggle
                    objectName: "switch"
                    Layout.fillWidth: true
                    text: qsTr("Use the alternate behavior")
                }

                C.Slider {
                    id: level
                    objectName: "slider"
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 25
                    stepSize: 10
                    accessibleName: qsTr("Example level")
                    accessibleDescription: qsTr("Choose an example level from zero to one hundred")
                }
            }
        }

        C.StateCard {
            id: stateCard
            objectName: "stateCard"
            Layout.fillWidth: true
            status: C.StateCard.Error
            title: qsTr("The change was not saved")
            message: qsTr("Review the highlighted value and try again.")
            actionText: qsTr("Review")
        }

        C.StateCard {
            id: busyStateCard
            objectName: "busyStateCard"
            Layout.fillWidth: true
            status: C.StateCard.Busy
            title: qsTr("Saving changes")
            message: qsTr("Wait while the example values are written.")
        }

        C.DegradedNotice {
            id: degraded
            objectName: "degradedNotice"
            Layout.fillWidth: true
            reason: qsTr("The example provider is not running.")
            retryText: qsTr("Retry")
        }

        C.ThemeCard {
            id: themeCard
            objectName: "themeCard"
            Layout.fillWidth: true
            themeName: qsTr("Current theme")
            description: qsTr("Complete active QST-1 generation")
        }

        C.TokenSwatch {
            id: swatch
            objectName: "tokenSwatch"
            label: qsTr("Accent")
            accessibleDescription: qsTr("The current semantic accent color")
        }

        C.Label {
            id: specimenLabel
            objectName: "specimenLabel"
            Layout.fillWidth: true
            text: qsTr("Muted supporting label")
            muted: true
        }
    }
}
