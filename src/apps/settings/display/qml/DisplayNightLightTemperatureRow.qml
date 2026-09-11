// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T2
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaQt.Shell.Icons 1.0 as ShellIcons

// One icon-led temperature row: slider plus kelvin readout. Shared by the
// night temperature row (which previews through KWin) and the day
// temperature row (no preview); the section decides by connecting the
// signals.
RowLayout {
    id: row

    required property var nightLight
    required property bool rowEnabled
    required property string rowIcon
    required property string rowObjectBase
    required property string rowLabel
    required property string rowDescription
    required property string sliderAccessibleDescription
    required property int kelvin

    signal kelvinMoved(int kelvin)
    signal withdrawRequested()

    // Current slider value in kelvin; the section's settle timer reads it
    // when it fires.
    readonly property real sliderValue: slider.value

    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    ShellIcons.Icon {
        objectName: row.rowObjectBase + "Icon"
        name: row.rowIcon
        size: 22
        Layout.alignment: Qt.AlignVCenter
    }

    FormRow {
        objectName: row.rowObjectBase + "Row"
        Layout.fillWidth: true
        label: row.rowLabel
        description: row.rowDescription
        editor: sliderHost

        RowLayout {
            id: sliderHost

            spacing: Tokens.space["3"]

            Slider {
                id: slider

                objectName: row.rowObjectBase + "Slider"
                enabled: row.rowEnabled
                Layout.preferredWidth: 260
                from: 1000
                to: 6500
                stepSize: 100
                value: row.kelvin
                accessibleName: row.rowLabel
                accessibleDescription: row.sliderAccessibleDescription
                onMoved: row.kelvinMoved(value)
                // AGENT-GUARD: QQuickSlider has no released signal; a drag
                // ends at the pressed false edge and keyboard use ends at
                // focus loss. Both withdraw the preview so the section never
                // leaves one running behind the user.
                onPressedChanged: {
                    if (!pressed) {
                        row.withdrawRequested()
                    }
                }
                onActiveFocusChanged: {
                    if (!activeFocus) {
                        row.withdrawRequested()
                    }
                }
                T2.ToolTip.visible: hovered || pressed
                T2.ToolTip.text: qsTr("%1 K").arg(value)
            }

            Label {
                objectName: row.rowObjectBase + "Value"
                text: qsTr("%1 K").arg(row.kelvin)
                Accessible.ignored: true
            }
        }
    }
}
