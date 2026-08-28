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

    Keys.onPressed: event => {
        const pageStep = Math.max(1, formViewport.height - Tokens.space["5"])
        if (event.key === Qt.Key_PageDown) {
            formViewport.contentY = Math.min(
                        Math.max(0, formViewport.contentHeight - formViewport.height),
                        formViewport.contentY + pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            formViewport.contentY = Math.max(0,
                                             formViewport.contentY - pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_Home
                   && (event.modifiers & Qt.ControlModifier)) {
            formViewport.contentY = 0
            event.accepted = true
        } else if (event.key === Qt.Key_End
                   && (event.modifiers & Qt.ControlModifier)) {
            formViewport.contentY = Math.max(
                        0, formViewport.contentHeight - formViewport.height)
            event.accepted = true
        }
    }

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
            objectName: "appearanceSaveResults"
            Layout.fillWidth: true
            visible: root.appearanceSettings.saveResultsText.length > 0
            text: root.appearanceSettings.saveResultsText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: root.appearanceSettings.saveResultsHaveFailure
                   ? Tokens.status.warning.foreground : Tokens.fg.muted
            Accessible.role: root.appearanceSettings.saveResultsHaveFailure
                             ? Accessible.AlertMessage : Accessible.StaticText
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
            id: formViewport
            objectName: "appearanceFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Appearance settings scroll position")
            }

            function revealItem(focusedItem) {
                if (focusedItem === null || focusedItem === undefined
                        || focusedItem === formViewport) {
                    return
                }
                let cursor = focusedItem
                let belongsToForm = false
                while (cursor !== null && cursor !== undefined) {
                    if (cursor === formSurface) {
                        belongsToForm = true
                        break
                    }
                    cursor = cursor.parent
                }
                if (!belongsToForm) {
                    return
                }
                const position = focusedItem.mapToItem(formSurface, 0, 0)
                const margin = Tokens.space["2"]
                const top = position.y - margin
                const bottom = position.y + focusedItem.height + margin
                if (top < contentY) {
                    contentY = Math.max(0, top)
                } else if (bottom > contentY + height) {
                    contentY = Math.min(Math.max(0, contentHeight - height),
                                        bottom - height)
                }
            }

            function revealActiveFocus() {
                if (root.Window.window !== null) {
                    revealItem(root.Window.window.activeFocusItem)
                }
            }

            onHeightChanged: Qt.callLater(revealActiveFocus)

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

        Connections {
            target: root.Window.window
            enabled: root.Window.window !== null
            function onActiveFocusItemChanged() {
                formViewport.revealActiveFocus()
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
