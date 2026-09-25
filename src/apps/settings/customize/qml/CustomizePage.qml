// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Settings → Customize: layout presets (ADR-0267). Clicking a card switches
// the desktop to that layout (the confirmed Settings1 selection the shell
// adopts live). Built-ins come first, then the user's own presets, which can
// be saved from the current layout, renamed, duplicated and deleted; an
// edited built-in shows Modified and can be restored or kept as a new preset.
// Layouts are edited on the panels themselves (Edit Panels, ADR-0266), so
// this page holds no draft and has nothing to apply.
T.Page {
    id: root

    required property var customizeSettings
    readonly property var presets: root.customizeSettings.presets ?? []
    readonly property var builtInPresets: root.presets.filter(preset => preset.builtIn === true)
    readonly property var ownPresets: root.presets.filter(preset => preset.own === true)
    readonly property Item firstFocusTarget: builtInSection.firstCard !== null
                                             ? builtInSection.firstCard : saveButton

    title: qsTr("Customize")
    background: Rectangle { color: Tokens.bg.base }

    function presetDisplayName(presetId) {
        const match = root.presets.find(preset => preset.id === presetId)
        return match !== undefined ? String(match.name) : ""
    }

    function handlePresetAction(action, preset) {
        const settings = root.customizeSettings
        if (action === "activate") {
            settings.activatePreset(preset.id)
        } else if (action === "rename") {
            nameDialog.openFor("rename", preset.id, preset.name)
        } else if (action === "saveAs") {
            nameDialog.openFor("saveAs", preset.id, preset.name)
        } else if (action === "duplicate") {
            settings.duplicatePreset(preset.id)
        } else if (action === "delete" || action === "restore") {
            confirmDialog.openFor(action, preset)
        }
    }

    T.ScrollView {
        id: scroller

        anchors.fill: parent
        padding: Tokens.space["4"]
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroller.availableWidth
            spacing: Tokens.space["4"]

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["3"]

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Customize")
                    font.pointSize: Tokens.type.title
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    objectName: "customizeStatus"
                    Layout.maximumWidth: scroller.availableWidth / 2
                    visible: !root.customizeSettings.unavailable
                    text: root.customizeSettings.statusText
                    muted: true
                    font.pointSize: Tokens.type.caption
                    horizontalAlignment: Text.AlignRight
                    Accessible.name: text
                }
            }

            Label {
                objectName: "customizeEditHint"
                Layout.fillWidth: true
                text: qsTr("To change a layout, right-click a panel and choose Edit Panels: drag applets where you want them, and use the panel and applet menus to add applets or move and resize a panel. Changes are saved to the current layout.")
                muted: true
                wrapMode: Text.Wrap
            }

            Label {
                objectName: "customizeError"
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.customizeSettings.errorText
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            Label {
                objectName: "customizeNotice"
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.customizeSettings.noticeText
                muted: true
                wrapMode: Text.Wrap
                Accessible.name: text
            }

            DegradedNotice {
                objectName: "customizeUnavailableNotice"
                Layout.fillWidth: true
                visible: root.customizeSettings.unavailable
                reason: root.customizeSettings.statusText
                retryText: qsTr("Retry")
                onRetryRequested: root.customizeSettings.retry()
            }

            CustomizePresetSection {
                id: builtInSection

                objectName: "customizeBuiltInPresets"
                Layout.fillWidth: true
                visible: !root.customizeSettings.unavailable
                customizeSettings: root.customizeSettings
                title: qsTr("Built-in layouts")
                presets: root.builtInPresets
                onPresetAction: (action, preset) => root.handlePresetAction(action, preset)
            }

            CustomizePresetSection {
                objectName: "customizeOwnPresets"
                Layout.fillWidth: true
                visible: !root.customizeSettings.unavailable
                customizeSettings: root.customizeSettings
                title: qsTr("My presets")
                presets: root.ownPresets
                emptyText: qsTr("Save the current layout to keep it as a preset.")
                onPresetAction: (action, preset) => root.handlePresetAction(action, preset)
            }

            Button {
                id: saveButton

                objectName: "customizeSaveCurrentPreset"
                visible: !root.customizeSettings.unavailable
                text: qsTr("Save current layout as preset…")
                emphasized: false
                available: root.customizeSettings.canManage === true
                           && root.customizeSettings.activePresetId.length > 0
                onClicked: nameDialog.openFor(
                               "save", root.customizeSettings.activePresetId,
                               root.presetDisplayName(root.customizeSettings.activePresetId))
            }

            CustomizePanelOptions {
                Layout.fillWidth: true
                customizeSettings: root.customizeSettings
            }
        }
    }

    CustomizePresetNameDialog {
        id: nameDialog
        customizeSettings: root.customizeSettings
    }

    // Delete (own presets) and Restore original (edited built-ins) both throw
    // work away for good, so both ask first.
    T.Dialog {
        id: confirmDialog

        property string action: "delete"
        property var preset: ({})
        readonly property string presetName: String(confirmDialog.preset.name ?? "")

        function openFor(nextAction, nextPreset) {
            confirmDialog.action = nextAction
            confirmDialog.preset = nextPreset
            confirmDialog.open()
        }

        objectName: "customizePresetConfirmDialog"
        title: confirmDialog.action === "restore" ? qsTr("Restore the original layout?")
                                                  : qsTr("Delete this preset?")
        modal: true
        anchors.centerIn: T.Overlay.overlay
        width: Math.min(460, (T.Overlay.overlay !== null ? T.Overlay.overlay.width : 460)
                             - 2 * Tokens.space["4"])

        contentItem: ColumnLayout {
            spacing: Tokens.space["3"]

            Label {
                objectName: "customizePresetConfirmText"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: confirmDialog.action === "restore"
                      ? qsTr("Your changes to “%1” are discarded and the original layout comes back. To keep them, save it as a new preset first.").arg(confirmDialog.presetName)
                      : confirmDialog.preset.active === true
                        ? qsTr("“%1” is the current layout. The desktop switches to %2 first, then the preset is deleted. This can't be undone.")
                              .arg(confirmDialog.presetName)
                              .arg(root.presetDisplayName(root.customizeSettings.defaultPresetId)
                                   || qsTr("the default layout"))
                        : qsTr("“%1” is deleted. This can't be undone.").arg(confirmDialog.presetName)
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["2"]

                Item { Layout.fillWidth: true }

                Button {
                    objectName: "customizePresetConfirmCancel"
                    text: qsTr("Cancel")
                    emphasized: false
                    onClicked: confirmDialog.close()
                }
                Button {
                    objectName: "customizePresetConfirmAccept"
                    text: confirmDialog.action === "restore" ? qsTr("Restore") : qsTr("Delete")
                    destructive: true
                    onClicked: {
                        const id = String(confirmDialog.preset.id ?? "")
                        confirmDialog.close()
                        if (confirmDialog.action === "restore")
                            root.customizeSettings.restorePreset(id)
                        else
                            root.customizeSettings.deletePreset(id)
                    }
                }
            }
        }
    }
}
