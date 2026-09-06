// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Keyboard-first placement for the obvious cases: put the selected display
// directly beside another one. Fine positioning stays with drag or arrows.
FormRow {
    id: root

    required property var outputs
    required property string selectedOutputId
    required property bool selectedEnabled
    required property bool canEdit

    signal placeRequested(string side, string referenceId)

    // Reassigned only when the candidates change so the reference selector
    // keeps its choice across ordinary draft edits.
    property var otherEnabled: []
    readonly property var referenceNames: root.otherEnabled.map(entry => entry.label)
    readonly property string referenceId: root.otherEnabled.length === 0 ? ""
                                          : root.otherEnabled[Math.min(
                                                referenceSelector.currentIndex < 0 ? 0
                                                : referenceSelector.currentIndex,
                                                root.otherEnabled.length - 1)].stableId
    readonly property bool placementAvailable: root.canEdit && root.selectedEnabled
                                               && root.otherEnabled.length > 0

    function refreshOtherEnabled() {
        const list = root.outputs ?? []
        const others = []
        for (let index = 0; index < list.length; ++index) {
            const output = list[index]
            if (output.enabled && output.stableId !== root.selectedOutputId) {
                others.push({ stableId: String(output.stableId),
                              label: qsTr("%1 %2").arg(index + 1)
                                     .arg(output.label ?? output.connectorName ?? "") })
            }
        }
        if (JSON.stringify(others) !== JSON.stringify(root.otherEnabled)) {
            root.otherEnabled = others
        }
    }

    onOutputsChanged: root.refreshOtherEnabled()
    onSelectedOutputIdChanged: root.refreshOtherEnabled()
    Component.onCompleted: root.refreshOtherEnabled()

    label: qsTr("Quick placement")
    description: root.otherEnabled.length === 0
                 ? qsTr("Enable a second display to place this one beside it.")
                 : qsTr("Put the selected display beside another one, then fine-tune by dragging or with the arrow keys.")
    editor: placementRow

    ColumnLayout {
        id: placementRow
        spacing: Tokens.space["2"]

        RowLayout {
            spacing: Tokens.space["2"]
            visible: root.otherEnabled.length > 1

            Label {
                text: qsTr("Relative to")
                muted: true
            }

            ComboBox {
                id: referenceSelector
                objectName: "displayPlacementReferenceSelector"
                Layout.preferredWidth: 220
                model: root.referenceNames
                enabled: root.placementAvailable
                accessibleDescription: qsTr("Display the quick placement is relative to")
            }
        }

        Label {
            visible: root.otherEnabled.length === 1
            objectName: "displayPlacementReferenceLabel"
            text: root.otherEnabled.length === 1
                  ? qsTr("Relative to %1").arg(root.otherEnabled[0].label) : ""
            muted: true
        }

        Flow {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Repeater {
                model: [
                    { side: "left", text: qsTr("Left of"), objectName: "displayPlaceLeftButton" },
                    { side: "right", text: qsTr("Right of"), objectName: "displayPlaceRightButton" },
                    { side: "above", text: qsTr("Above"), objectName: "displayPlaceAboveButton" },
                    { side: "below", text: qsTr("Below"), objectName: "displayPlaceBelowButton" }
                ]

                delegate: Button {
                    id: placementButton
                    required property var modelData

                    objectName: placementButton.modelData.objectName
                    text: placementButton.modelData.text
                    emphasized: false
                    available: root.placementAvailable
                    accessibleDescription: qsTr("Move the selected display %1 the reference display")
                                           .arg(placementButton.modelData.text.toLowerCase())
                    onClicked: {
                        if (root.referenceId.length > 0) {
                            root.placeRequested(placementButton.modelData.side, root.referenceId)
                        }
                    }
                }
            }
        }
    }
}
