// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var networkSettings
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Radios")
        description: qsTr("Observed hardware and software radio state; radio changes are outside this page")
    }

    Repeater {
        model: root.networkSettings.radios

        delegate: FormRow {
            required property var modelData
            Layout.fillWidth: true
            label: modelData.name
            description: modelData.statusText
            editor: Label {
                text: modelData.statusText
                muted: true
                Accessible.name: qsTr("%1 radio: %2").arg(modelData.name)
                                                     .arg(modelData.statusText)
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: root.networkSettings.radios.length === 0
        text: qsTr("No radio inventory is available.")
        muted: true
    }
}
