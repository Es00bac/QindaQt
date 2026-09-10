// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Applet inspector: identity with glyph, a zone selector drawn as three
// panel glyphs, duplicate/remove icon actions, and the manifest's settings
// schema as a quiet read-only list (the accepted editor vocabulary has no
// arbitrary applet-settings intent yet).
FormSurface {
    id: root

    required property var customizeSettings
    required property var properties

    readonly property var settingsFields: root.properties.settingsFields ?? []

    ColumnLayout {
        width: parent.width
        spacing: Tokens.space["3"]

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["3"]

            Rectangle {
                Layout.preferredWidth: Tokens.space["6"]
                Layout.preferredHeight: Tokens.space["6"]
                radius: Tokens.radius.m
                color: Tokens.bg.highest
                border.width: Tokens.space["1"] / 2
                border.color: Tokens.outline.divider

                CustomizeAppletIcon {
                    anchors.centerIn: parent
                    pluginId: root.properties.pluginId ?? ""
                    iconSize: 20
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["1"]

                Label {
                    Layout.fillWidth: true
                    text: root.properties.name ?? qsTr("Applet")
                    font.weight: Font.DemiBold
                    font.pointSize: Tokens.type.subtitle
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    visible: (root.properties.pluginId ?? "").length > 0
                    text: root.properties.pluginId ?? ""
                    muted: true
                    font.pointSize: Tokens.type.caption
                    font.family: Tokens.type.monoFontFamily
                    elide: Text.ElideMiddle
                    Accessible.name: text
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Zone")
            muted: true
            font.pointSize: Tokens.type.caption
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: Tokens.space["1"]

            Repeater {
                model: [
                    { token: "start", glyph: "zone-start", tip: qsTr("Leading zone") },
                    { token: "center", glyph: "zone-center", tip: qsTr("Center zone") },
                    { token: "end", glyph: "zone-end", tip: qsTr("Trailing zone") }
                ]

                delegate: CustomizeIconButton {
                    id: zoneButton

                    required property var modelData

                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    glyphName: modelData.glyph
                    toolTip: modelData.tip
                    available: root.customizeSettings.canEdit
                    emphasized: (root.properties.zone ?? "start")
                                === modelData.token
                    onClicked: {
                        // Zone moves reuse the one public editor gesture:
                        // arm, target, commit-or-cancel. There is no separate
                        // move intent, so buttons and drags converge on the
                        // same durable Undo step.
                        const s = root.customizeSettings
                        if (!s.startAppletDrag(s.selectedPanelId,
                                               s.selectedAppletId))
                            return
                        if (s.hoverDropTarget(s.selectedPanelId,
                                              modelData.token, "")
                            && s.dropAccepted)
                            s.commitDrag()
                        else
                            s.cancelDrag()
                    }
                    Accessible.role: Accessible.RadioButton
                    Accessible.checked: (root.properties.zone ?? "start")
                                         === modelData.token
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            CustomizeIconButton {
                objectName: "customizeDuplicateButton"
                iconName: "edit-copy"
                toolTip: qsTr("Duplicate this applet")
                available: root.customizeSettings.canEdit
                onClicked: root.customizeSettings.duplicateSelected()
            }

            CustomizeIconButton {
                objectName: "customizeRemoveButton"
                iconName: "edit-delete"
                toolTip: qsTr("Remove this applet")
                destructive: true
                available: root.customizeSettings.canEdit
                onClicked: root.customizeSettings.removeSelected()
            }

            Item { Layout.fillWidth: true }
        }

        Label {
            Layout.fillWidth: true
            visible: root.settingsFields.length > 0
            text: qsTr("Applet settings")
            muted: true
            font.pointSize: Tokens.type.caption
            Accessible.name: text
        }

        Repeater {
            model: root.settingsFields

            delegate: RowLayout {
                id: fieldRow

                required property var modelData

                width: parent.width
                spacing: Tokens.space["2"]

                Label {
                    Layout.fillWidth: true
                    text: fieldRow.modelData.label ?? ""
                    muted: true
                    font.pointSize: Tokens.type.caption
                    elide: Text.ElideRight
                    Accessible.name: text
                }

                Label {
                    text: String(fieldRow.modelData.value ?? "—")
                    font.pointSize: Tokens.type.caption
                    font.family: Tokens.type.monoFontFamily
                    elide: Text.ElideMiddle
                    Accessible.name: qsTr("%1: %2").arg(
                        fieldRow.modelData.label ?? "").arg(text)
                    Accessible.description: qsTr(
                        "Read-only schema field of type %1").arg(
                        fieldRow.modelData.type ?? "")
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.settingsFields.length === 0
            text: root.properties.schemaAvailable
                  ? qsTr("This applet has no configurable fields")
                  : qsTr("This applet has no settings available")
            muted: true
            font.pointSize: Tokens.type.caption
            wrapMode: Text.Wrap
            Accessible.name: text
        }
    }
}
