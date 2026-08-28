// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

Rectangle {
    id: gallery

    readonly property bool compactFixture: width < 600

    width: 720
    height: 840
    color: Tokens.bg.base
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: gallery.compactFixture ? Tokens.space["4"]
                                                : Tokens.space["6"]
        spacing: gallery.compactFixture ? Tokens.space["4"] : Tokens.space["5"]

        C.SectionHeader {
            Layout.fillWidth: true
            title: qsTr("QindaQt Controls")
            description: qsTr("One semantic component set across every built-in desktop theme.")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: gallery.width >= 900 ? 3 : gallery.width >= 600 ? 2 : 1
            columnSpacing: Tokens.space["4"]
            rowSpacing: gallery.compactFixture ? Tokens.space["3"] : Tokens.space["4"]

            C.ThemeCard {
                Layout.fillWidth: true
                themeName: qsTr("Selected appearance")
                description: qsTr("Complete QST-1 preview")
                checked: true
            }

            C.StateCard {
                objectName: "galleryErrorStateCard"
                Layout.fillWidth: true
                status: C.StateCard.Error
                title: qsTr("Needs attention")
                message: qsTr("Review the highlighted value before applying changes.")
            }

            C.DegradedNotice {
                objectName: "galleryDegradedNotice"
                Layout.fillWidth: true
                reason: qsTr("A demonstration provider is temporarily unavailable.")
                retryText: qsTr("Retry")
            }
        }

        C.FormSurface {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                width: parent.width
                spacing: Tokens.space["3"]

                C.FormRow {
                    id: nameRow
                    Layout.fillWidth: true
                    label: qsTr("Display name")
                    description: qsTr("Shown to people using this desktop.")
                    required: true
                    errorMessage: qsTr("Enter a display name before continuing.")
                    editor: nameField

                    C.TextField {
                        id: nameField
                        width: parent.width
                        text: qsTr("QindaQt workstation")
                        error: true
                    }
                }

                C.FormRow {
                    id: scaleRow
                    Layout.fillWidth: true
                    label: qsTr("Interface scale")
                    description: qsTr("Changes the size of controls without changing their meaning.")
                    editor: scaleSlider

                    C.Slider {
                        id: scaleSlider
                        objectName: "galleryScaleSlider"
                        width: parent.width
                        from: 0.5
                        to: 2.0
                        value: 1.0
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Tokens.space["4"]

                    C.CheckBox {
                        text: qsTr("Show helpful descriptions")
                        checked: true
                    }

                    C.Switch {
                        text: qsTr("Use smooth transitions")
                        checked: true
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: childrenRect.height
                    spacing: Tokens.space["3"]

                    C.TokenSwatch {
                        label: qsTr("Accent")
                    }

                    C.Button {
                        text: qsTr("Unavailable")
                        emphasized: false
                        available: false
                        error: true
                        accessibleDescription: qsTr("Correct the highlighted value first")
                    }

                    C.Button {
                        text: qsTr("Save")
                        busy: true
                    }

                    C.Button {
                        text: qsTr("Apply")
                    }
                }
            }
        }
    }
}
