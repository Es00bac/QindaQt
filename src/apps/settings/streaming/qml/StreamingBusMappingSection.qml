// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Which console buses and strips OBS can record, as the QindaQt bridge
// plugin reports them.
//
// AGENT-CONTRACT: every row here comes from the bridge's obs-websocket
// vendor request, never from parsing an OBS source name — the source name is
// a display string that changes when the user renames a bus.
ColumnLayout {
    id: root

    required property var streamingSettings

    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Console buses in OBS")
        description: qsTr("The mixing console's buses and strips, as OBS sources")
    }

    Label {
        objectName: "streamingBridgeProblem"
        Layout.fillWidth: true
        visible: text.length > 0
        muted: true
        text: root.streamingSettings.bridgeProblem
    }

    Repeater {
        model: root.streamingSettings.busMapping

        delegate: RowLayout {
            id: mappingRow
            required property var modelData
            objectName: "streamingBusRow_" + modelData.consoleId
            Layout.fillWidth: true
            spacing: Tokens.space["3"]

            Label {
                Layout.preferredWidth: 72
                text: mappingRow.modelData.kind
                muted: true
            }
            Label {
                Layout.preferredWidth: 110
                text: mappingRow.modelData.code
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: mappingRow.modelData.sourceName
            }
            Label {
                visible: mappingRow.modelData.muted
                text: qsTr("muted")
                muted: true
            }
        }
    }

    Button {
        objectName: "streamingRefreshMappingButton"
        text: qsTr("Refresh")
        emphasized: false
        available: root.streamingSettings.connected
        accessibleDescription: qsTr("Ask OBS for the console mapping again")
        onClicked: root.streamingSettings.refreshBusMapping()
    }
}
