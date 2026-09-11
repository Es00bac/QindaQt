// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaQt.Shell.Icons 1.0 as ShellIcons

// Manual-location row: latitude and longitude fields for the "sunset to
// sunrise at coordinates" schedule. Values resynchronize from the model
// whenever the user is not mid-edit, so external draft refreshes are never
// clobbered by a stale text binding.
RowLayout {
    id: root

    required property var nightLight
    required property bool rowEnabled

    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    ShellIcons.Icon {
        objectName: "nightLightLocationIcon"
        name: "mark-location-symbolic"
        size: 22
        Layout.alignment: Qt.AlignVCenter
    }

    FormRow {
        objectName: "nightLightLocationRow"
        Layout.fillWidth: true
        label: qsTr("Location")
        description: qsTr("Coordinates the schedule follows.")
        editor: nightLightLocationFields

        RowLayout {
            id: nightLightLocationFields

            enabled: root.rowEnabled
            spacing: Tokens.space["2"]

            TextField {
                id: latitudeField

                objectName: "nightLightLatitudeField"
                Layout.preferredWidth: 120
                text: Number(
                          root.nightLight.draftLatitude).toLocaleString(
                          Qt.locale(), 'f', 2)
                onTextEdited: userDirty = true
                onEditingFinished: {
                    if (!userDirty) {
                        return
                    }
                    userDirty = false
                    const value = Number.fromLocaleString(Qt.locale(), text)
                    if (!Number.isNaN(value) && value >= -90.0
                        && value <= 90.0) {
                        root.nightLight.draftLatitude = value
                    }
                }
                validator: DoubleValidator {
                    bottom: -90.0
                    top: 90.0
                    decimals: 4
                    notation: DoubleValidator.StandardNotation
                }
                accessibleDescription: qsTr(
                    "Latitude from minus ninety to ninety degrees.")
                T.ToolTip.visible: hovered && latitudeField.enabled
                T.ToolTip.text: qsTr("Latitude, −90 to 90 degrees.")

                property bool userDirty: false

                Connections {
                    target: root.nightLight
                    function onDraftChanged() {
                        if (!latitudeField.userDirty
                            && !latitudeField.activeFocus) {
                            latitudeField.text = Number(
                                root.nightLight.draftLatitude).toLocaleString(
                                Qt.locale(), 'f', 2)
                        }
                    }
                }
            }

            TextField {
                id: longitudeField

                objectName: "nightLightLongitudeField"
                Layout.preferredWidth: 120
                text: Number(
                          root.nightLight.draftLongitude).toLocaleString(
                          Qt.locale(), 'f', 2)
                onTextEdited: userDirty = true
                onEditingFinished: {
                    if (!userDirty) {
                        return
                    }
                    userDirty = false
                    const value = Number.fromLocaleString(Qt.locale(), text)
                    if (!Number.isNaN(value) && value >= -180.0
                        && value <= 180.0) {
                        root.nightLight.draftLongitude = value
                    }
                }
                validator: DoubleValidator {
                    bottom: -180.0
                    top: 180.0
                    decimals: 4
                    notation: DoubleValidator.StandardNotation
                }
                accessibleDescription: qsTr(
                    "Longitude from minus one hundred eighty to one hundred eighty degrees.")
                T.ToolTip.visible: hovered && longitudeField.enabled
                T.ToolTip.text: qsTr("Longitude, −180 to 180 degrees.")

                property bool userDirty: false

                Connections {
                    target: root.nightLight
                    function onDraftChanged() {
                        if (!longitudeField.userDirty
                            && !longitudeField.activeFocus) {
                            longitudeField.text = Number(
                                root.nightLight.draftLongitude).toLocaleString(
                                Qt.locale(), 'f', 2)
                        }
                    }
                }
            }
        }
    }
}
