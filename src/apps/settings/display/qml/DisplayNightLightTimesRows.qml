// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaQt.Shell.Icons 1.0 as ShellIcons

// Custom-times rows: sunset/sunrise time fields plus the transition length.
// Time values resynchronize from the model whenever the user is not
// mid-edit, so external draft refreshes never clobber in-flight input.
ColumnLayout {
    id: root

    required property var nightLight
    required property bool rowEnabled

    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["3"]

        ShellIcons.Icon {
            objectName: "nightLightTimesIcon"
            name: "appointment-new-symbolic"
            size: 22
            Layout.alignment: Qt.AlignVCenter
        }

        FormRow {
            objectName: "nightLightTimesRow"
            Layout.fillWidth: true
            label: qsTr("Custom times")
            description: qsTr("Warm between sunset and sunrise times.")
            editor: nightLightTimesFields

            RowLayout {
                id: nightLightTimesFields

                enabled: root.rowEnabled
                spacing: Tokens.space["2"]

                TextField {
                    id: sunriseField

                    objectName: "nightLightSunriseField"
                    Layout.preferredWidth: 96
                    text: root.nightLight.draftSunrise.toLocaleString(
                              Qt.locale(), "HH:mm")
                    onTextEdited: userDirty = true
                    onEditingFinished: {
                        if (!userDirty) {
                            return
                        }
                        userDirty = false
                        const value = QTime.fromLocaleString(Qt.locale(),
                                                             text, "HH:mm")
                        if (value.isValid()) {
                            root.nightLight.draftSunrise = value
                        }
                    }
                    validator: RegularExpressionValidator {
                        regularExpression: /^([01]\d|2[0-3]):[0-5]\d$/
                    }
                    accessibleDescription: qsTr("Sunrise time, 24-hour.")
                    T.ToolTip.visible: hovered && sunriseField.enabled
                    T.ToolTip.text: qsTr("Morning time the display returns to day temperature.")

                    property bool userDirty: false

                    Connections {
                        target: root.nightLight
                        function onDraftChanged() {
                            if (!sunriseField.userDirty
                                && !sunriseField.activeFocus) {
                                sunriseField.text =
                                    root.nightLight.draftSunrise.toLocaleString(
                                    Qt.locale(), "HH:mm")
                            }
                        }
                    }
                }

                Label {
                    text: qsTr("to")
                    muted: true
                    Accessible.ignored: true
                }

                TextField {
                    id: sunsetField

                    objectName: "nightLightSunsetField"
                    Layout.preferredWidth: 96
                    text: root.nightLight.draftSunset.toLocaleString(
                              Qt.locale(), "HH:mm")
                    onTextEdited: userDirty = true
                    onEditingFinished: {
                        if (!userDirty) {
                            return
                        }
                        userDirty = false
                        const value = QTime.fromLocaleString(Qt.locale(),
                                                             text, "HH:mm")
                        if (value.isValid()) {
                            root.nightLight.draftSunset = value
                        }
                    }
                    validator: RegularExpressionValidator {
                        regularExpression: /^([01]\d|2[0-3]):[0-5]\d$/
                    }
                    accessibleDescription: qsTr("Sunset time, 24-hour.")
                    T.ToolTip.visible: hovered && sunsetField.enabled
                    T.ToolTip.text: qsTr("Evening time the display starts warming.")

                    property bool userDirty: false

                    Connections {
                        target: root.nightLight
                        function onDraftChanged() {
                            if (!sunsetField.userDirty
                                && !sunsetField.activeFocus) {
                                sunsetField.text =
                                    root.nightLight.draftSunset.toLocaleString(
                                    Qt.locale(), "HH:mm")
                            }
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["3"]

        ShellIcons.Icon {
            objectName: "nightLightTransitionIcon"
            name: "input-keyboard-brightness"
            size: 22
            Layout.alignment: Qt.AlignVCenter
        }

        FormRow {
            objectName: "nightLightTransitionRow"
            Layout.fillWidth: true
            label: qsTr("Transition length")
            description: qsTr("How long warming takes.")
            editor: nightLightTransitionCombo

            ComboBox {
                id: nightLightTransitionCombo

                objectName: "nightLightTransitionCombo"
                enabled: root.rowEnabled
                model: [5, 15, 30, 45, 60, 90, 120]
                currentIndex: {
                    const index = model.indexOf(
                        root.nightLight.draftTransitionMinutes)
                    return index >= 0 ? index : 2
                }
                displayText: qsTr("%1 minutes").arg(currentValue)
                onActivated: function(index) {
                    root.nightLight.draftTransitionMinutes = currentValue
                }
                accessibleDescription: qsTr(
                    "How many minutes the warm-up takes.")
                T.ToolTip.visible: hovered
                                   && nightLightTransitionCombo.enabled
                T.ToolTip.text: qsTr("Between one and one hundred twenty minutes.")
            }
        }
    }
}
