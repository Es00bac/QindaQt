// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})

    objectName: "powerApplet"
    implicitWidth: vertical ? 40 : Math.max(46, summary.implicitWidth + 12)
    implicitHeight: vertical ? 40 : 28

    ToolButton {
        id: summary
        objectName: "powerAppletSummary"
        anchors.fill: parent
        enabled: root.access !== null
        focusPolicy: Qt.TabFocus
        text: root.access !== null ? root.access.batteryLabel : qsTr("Power")
        Accessible.role: Accessible.Button
        Accessible.name: root.access !== null
                         ? root.access.accessibleName
                         : qsTr("Power information is unavailable")
        Accessible.description: root.access !== null
                                ? root.access.accessibleDescription : ""

        onClicked: details.open()
        Accessible.onPressAction: details.open()

        contentItem: Text {
            text: summary.text
            color: root.colors.text ?? "white"
            font.pixelSize: root.vertical ? 10 : 11
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
        }
        background: Item {}
    }

    Popup {
        id: details
        objectName: "powerAppletPopup"
        width: 300
        padding: 12
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: root.colors.surfaceRaised ?? "#2c312e"
            border.color: root.colors.border ?? "#3c433f"
        }

        contentItem: ColumnLayout {
            spacing: 8

            Label {
                objectName: "powerAppletHeading"
                Layout.fillWidth: true
                text: root.access !== null ? root.access.accessibleName
                                           : qsTr("Power unavailable")
                color: root.colors.text ?? "white"
                wrapMode: Text.Wrap
                Accessible.role: Accessible.Heading
            }

            Label {
                Layout.fillWidth: true
                visible: root.access !== null && root.access.diagnostic !== ""
                text: visible ? root.access.diagnostic : ""
                color: root.colors.textMuted ?? "#a9afa9"
                wrapMode: Text.Wrap
            }

            Repeater {
                model: root.access !== null ? root.access.keyboardRows : []

                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: parent.modelData.accessibleName
                        color: root.colors.text ?? "white"
                        elide: Text.ElideRight
                    }

                    Slider {
                        objectName: "powerAppletKeyboardSlider"
                        Layout.fillWidth: true
                        from: 0
                        to: 10000
                        stepSize: 100
                        value: parent.modelData.currentKnown
                               ? parent.modelData.normalizedCurrent : 0
                        enabled: parent.modelData.adjustable
                                 && !parent.modelData.pending
                        focusPolicy: Qt.TabFocus
                        Accessible.name: parent.modelData.accessibleName
                        Accessible.description:
                            parent.modelData.accessibleDescription
                        onMoved: root.access.requestKeyboardBrightness(
                                     parent.modelData.controlId,
                                     Math.round(value))
                    }
                }
            }

            Label {
                objectName: "powerAppletFeedback"
                Layout.fillWidth: true
                visible: root.access !== null && root.access.feedbackPresent
                text: visible ? root.access.feedback : ""
                color: root.colors.warning ?? "#e5a84b"
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
            }
        }
    }
}
