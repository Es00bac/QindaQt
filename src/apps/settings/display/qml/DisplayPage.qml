// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Display route composition: outputs inventory, active selection, resolution,
// scaling, orientation, arrangement, and bounded reversible transaction preview.
T.Page {
    id: root

    required property var displaySettings
    signal closeRequested()

    readonly property bool editorBusy: displaySettings.busy
                                     || displaySettings.loading
    readonly property Item firstFocusTarget: outputSection.firstFocusTarget

    title: qsTr("Display")

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
            objectName: "displayPageHeading"
            Layout.fillWidth: true
            text: qsTr("Display")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            objectName: "displayStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.displaySettings.statusText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: root.displaySettings.unavailable
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "displayError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.displaySettings.errorText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: Tokens.status.warning.foreground
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Repeater {
            model: root.displaySettings.warnings

            delegate: Label {
                required property var modelData
                Layout.fillWidth: true
                text: modelData
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                color: Tokens.status.warning.foreground
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }

        DisplayPreviewBanner {
            id: previewBanner
            objectName: "displayPreviewBanner"
            Layout.fillWidth: true
            displaySettings: root.displaySettings
        }

        DegradedNotice {
            id: unavailableNotice
            objectName: "displayUnavailableNotice"
            Layout.fillWidth: true
            visible: root.displaySettings.unavailable
            reason: root.displaySettings.statusText.length > 0
                    ? root.displaySettings.statusText
                    : qsTr("Display management service is unavailable.")
            retryText: qsTr("Retry Connection")
            onRetryRequested: root.displaySettings.retry()
        }

        Flickable {
            id: formViewport
            objectName: "displayFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.displaySettings.unavailable
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Display settings scroll position")
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
                    spacing: Tokens.space["4"]

                    DisplayOutputSection {
                        id: outputSection
                        displaySettings: root.displaySettings
                        editorBusy: root.editorBusy
                    }

                    DisplayModeSection {
                        id: modeSection
                        displaySettings: root.displaySettings
                        editorBusy: root.editorBusy
                    }

                    DisplayScaleSection {
                        id: scaleSection
                        displaySettings: root.displaySettings
                        editorBusy: root.editorBusy
                    }

                    DisplayTransformSection {
                        id: transformSection
                        displaySettings: root.displaySettings
                        editorBusy: root.editorBusy
                    }

                    DisplayArrangementSection {
                        id: arrangementSection
                        displaySettings: root.displaySettings
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
                objectName: "displayRetryButton"
                visible: root.displaySettings.unavailable
                available: !root.editorBusy
                text: qsTr("Retry")
                onClicked: root.displaySettings.retry()
            }

            Button {
                id: revertButton
                objectName: "displayRevertButton"
                visible: root.displaySettings.draftDirty
                         && !root.displaySettings.inTransaction
                available: root.displaySettings.canEdit
                text: qsTr("Revert")
                onClicked: root.displaySettings.cancelDraft()
            }

            Button {
                id: applyButton
                objectName: "displayApplyButton"
                visible: !root.displaySettings.inTransaction
                available: root.displaySettings.applyAvailable
                emphasized: true
                text: qsTr("Apply")
                onClicked: root.displaySettings.applyDraft()
            }

            Button {
                id: closeButton
                objectName: "displayCloseButton"
                available: !root.editorBusy
                text: qsTr("Close")
                KeyNavigation.tab: root.firstFocusTarget !== null ? root.firstFocusTarget : closeButton
                KeyNavigation.backtab: applyButton.visible ? applyButton
                                       : revertButton.visible ? revertButton
                                       : retryButton.visible ? retryButton
                                       : applyButton
                onClicked: root.closeRequested()
            }
        }
    }
}
