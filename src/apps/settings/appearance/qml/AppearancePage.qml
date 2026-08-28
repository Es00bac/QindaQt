// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Appearance route composition only. Cohesive preference sections own their
// controls; this page owns status, scrolling, and the draft action boundary.
T.Page {
    id: root

    required property var appearanceSettings
    signal closeRequested()

    readonly property bool editorBusy: appearanceSettings.saving
                                     || appearanceSettings.loading
    readonly property Item firstThemeCard: themeSection.firstThemeCard

    title: qsTr("Appearance")

    background: Rectangle {
        color: Tokens.bg.base
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "appearancePageHeading"
            Layout.fillWidth: true
            text: qsTr("Appearance")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            objectName: "appearanceStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.appearanceSettings.statusText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: root.appearanceSettings.conflict
                             || root.appearanceSettings.unavailable
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "appearanceError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.appearanceSettings.errorText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: Tokens.status.warning.foreground
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Label {
            objectName: "appearanceThemeFallback"
            Layout.fillWidth: true
            visible: root.appearanceSettings.fallbackNotice.length > 0
            text: root.appearanceSettings.fallbackNotice
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: Tokens.fg.muted
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Flickable {
            objectName: "appearanceFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight

            FormSurface {
                id: formSurface
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["2"]

                    AppearanceThemeSection {
                        id: themeSection
                        appearanceSettings: root.appearanceSettings
                        editorBusy: root.editorBusy
                    }

                    AppearanceFontSection {
                        appearanceSettings: root.appearanceSettings
                        editorBusy: root.editorBusy
                    }

                    AppearanceDesktopSection {
                        appearanceSettings: root.appearanceSettings
                        editorBusy: root.editorBusy
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Item { Layout.fillWidth: true }

            Button {
                id: retryButton
                objectName: "appearanceRetryButton"
                visible: root.appearanceSettings.unavailable
                available: !root.editorBusy
                text: qsTr("Retry")
                onClicked: root.appearanceSettings.retry()
            }

            Button {
                id: revertButton
                objectName: "appearanceRevertButton"
                visible: root.appearanceSettings.draftDirty
                         && !root.appearanceSettings.saving
                available: root.appearanceSettings.canEdit
                text: qsTr("Revert")
                onClicked: root.appearanceSettings.cancelDraft()
            }

            Button {
                id: applyButton
                objectName: "appearanceApplyButton"
                visible: !root.appearanceSettings.saving
                available: root.appearanceSettings.applyAvailable
                emphasized: true
                text: qsTr("Apply")
                onClicked: root.appearanceSettings.applyDraft()
            }

            Button {
                id: closeButton
                objectName: "appearanceCloseButton"
                available: !root.editorBusy
                text: qsTr("Close")
                KeyNavigation.tab: root.firstThemeCard
                KeyNavigation.backtab: applyButton.visible ? applyButton
                                       : revertButton.visible ? revertButton
                                       : retryButton.visible ? retryButton
                                       : applyButton
                onClicked: root.closeRequested()
            }
        }
    }
}
