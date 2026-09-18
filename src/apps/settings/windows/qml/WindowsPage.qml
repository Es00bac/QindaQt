// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The Windows & workspaces route: four Settings1 windowManagement keys that
// the session bridge carries into kwinrc live (ADR-0199). Every control edits
// the model's draft; nothing is written until Apply.
T.Page {
    id: root

    required property var windowsSettings
    signal closeRequested()

    // AGENT-GUARD: Focus entry must nominate an admitted domain control; the
    // window chrome owns closing, so the page does not duplicate it.
    readonly property Item firstFocusTarget: focusPolicyRow.selector.enabled ? focusPolicyRow.selector
                                             : retryButton.visible ? retryButton
                                                                   : root
    readonly property bool compact: width < 560
    // AGENT-NOTE: Explanations live in tooltips and accessible descriptions,
    // not in visible paragraphs (product direction: short labels, minimal
    // prose). Keep each description one sentence.
    readonly property string focusPolicyHelp: qsTr("How a window becomes active: by click, or by the pointer entering it.")
    readonly property string dockingModifierHelp: qsTr("Keys held with a left drag to dock, split, or group windows; Shift is always part of the chord.")
    readonly property string snapDistanceHelp: qsTr("Distance in pixels at which a dragged window snaps to screen edges and other windows. %1 is the default.")
        .arg(root.windowsSettings.defaultSnapDistance)
    readonly property string closePolicyHelp: qsTr("What closing a window group does without asking.")

    title: qsTr("Windows & workspaces")
    background: Rectangle { color: Tokens.bg.base }

    Keys.onPressed: event => {
        const pageStep = Math.max(1, viewport.height - Tokens.space["5"])
        if (event.key === Qt.Key_PageDown) {
            viewport.contentY = Math.min(
                        Math.max(0, viewport.contentHeight - viewport.height),
                        viewport.contentY + pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            viewport.contentY = Math.max(0, viewport.contentY - pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_Home
                   && (event.modifiers & Qt.ControlModifier)) {
            viewport.contentY = 0
            event.accepted = true
        } else if (event.key === Qt.Key_End
                   && (event.modifiers & Qt.ControlModifier)) {
            viewport.contentY = Math.max(
                        0, viewport.contentHeight - viewport.height)
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "windowsPageHeading"
            Layout.fillWidth: true
            text: qsTr("Windows & workspaces")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "windowsFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Window settings scroll position")
            }

            FormSurface {
                id: formSurface
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["3"]

                    DegradedNotice {
                        objectName: "windowsDegradedNotice"
                        Layout.fillWidth: true
                        visible: root.windowsSettings.unavailable
                        reason: root.windowsSettings.statusText
                        retryText: qsTr("Retry")
                        onRetryRequested: root.windowsSettings.retry()
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Focus")
                    }

                    WindowsChoiceRow {
                        id: focusPolicyRow
                        objectName: "windowsFocusPolicyRow"
                        label: qsTr("Window focus")
                        choices: root.windowsSettings.focusPolicyChoices
                        currentToken: root.windowsSettings.draftFocusPolicy
                        editable: root.windowsSettings.canEdit
                        help: root.focusPolicyHelp
                        selectorObjectName: "windowsFocusPolicySelector"
                        compact: root.compact
                        onChoicePicked: token => root.windowsSettings.setDraftFocusPolicy(token)
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Arranging windows")
                    }

                    WindowsChoiceRow {
                        objectName: "windowsDockingModifierRow"
                        label: qsTr("Docking drag")
                        choices: root.windowsSettings.dockingModifierChoices
                        currentToken: root.windowsSettings.draftDockingModifier
                        editable: root.windowsSettings.canEdit
                        help: root.dockingModifierHelp
                        selectorObjectName: "windowsDockingModifierSelector"
                        compact: root.compact
                        onChoicePicked: token => root.windowsSettings.setDraftDockingModifier(token)
                    }

                    GridLayout {
                        objectName: "windowsSnapDistanceRow"
                        Layout.fillWidth: true
                        columns: root.compact ? 1 : 3
                        columnSpacing: Tokens.space["3"]
                        rowSpacing: Tokens.space["2"]

                        Label {
                            text: qsTr("Snap distance")
                            Accessible.ignored: true
                        }

                        Slider {
                            id: snapDistanceSlider
                            objectName: "windowsSnapDistanceSlider"
                            Layout.fillWidth: true
                            from: root.windowsSettings.minimumSnapDistance
                            to: root.windowsSettings.maximumSnapDistance
                            stepSize: 1
                            snapMode: T.Slider.SnapAlways
                            enabled: root.windowsSettings.canEdit
                            accessibleName: qsTr("Snap distance")
                            accessibleDescription: root.snapDistanceHelp
                            T.ToolTip.text: root.snapDistanceHelp
                            T.ToolTip.visible: hovered
                            T.ToolTip.delay: 500
                            // Only an interactive move writes the draft; a
                            // confirmed refresh can never replay a value.
                            onMoved: root.windowsSettings.setDraftSnapDistance(Math.round(value))
                        }

                        // AGENT-NOTE: A plain `value:` binding would break on
                        // the first drag; this element re-asserts the model's
                        // draft whenever the handle is not pressed so Revert
                        // and remote changes move the slider.
                        Binding {
                            target: snapDistanceSlider
                            property: "value"
                            value: root.windowsSettings.draftSnapDistance
                            when: !snapDistanceSlider.pressed
                            restoreMode: Binding.RestoreBindingOrValue
                        }

                        Label {
                            objectName: "windowsSnapDistanceValue"
                            text: qsTr("%1 px").arg(Math.round(snapDistanceSlider.value))
                            muted: true
                            Accessible.ignored: true
                        }
                    }

                    Label {
                        objectName: "windowsSnapDistanceError"
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.windowsSettings.snapDistanceError
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Window groups")
                    }

                    WindowsChoiceRow {
                        objectName: "windowsClosePolicyRow"
                        label: qsTr("Closing a group")
                        choices: root.windowsSettings.closeContainerPolicyChoices
                        currentToken: root.windowsSettings.draftCloseContainerPolicy
                        editable: root.windowsSettings.canEdit
                        help: root.closePolicyHelp
                        selectorObjectName: "windowsClosePolicySelector"
                        compact: root.compact
                        onChoicePicked: token => root.windowsSettings.setDraftCloseContainerPolicy(token)
                    }

                    Label {
                        objectName: "windowsStatus"
                        Layout.fillWidth: true
                        visible: text.length > 0 && !root.windowsSettings.unavailable
                        text: root.windowsSettings.statusText
                        muted: true
                        Accessible.role: root.windowsSettings.conflict
                                         ? Accessible.AlertMessage
                                         : Accessible.StaticText
                        Accessible.name: text
                    }

                    Label {
                        objectName: "windowsError"
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.windowsSettings.errorText
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    RowLayout {
                        objectName: "windowsActionRow"
                        Layout.fillWidth: true
                        spacing: Tokens.space["3"]

                        Button {
                            id: applyButton
                            objectName: "windowsApplyButton"
                            text: qsTr("Apply")
                            available: root.windowsSettings.applyAvailable
                            busy: root.windowsSettings.saving
                            accessibleDescription: qsTr("Save the changed window settings")
                            onClicked: root.windowsSettings.applyDraft()
                        }

                        Button {
                            id: revertButton
                            objectName: "windowsRevertButton"
                            text: qsTr("Revert")
                            emphasized: false
                            available: root.windowsSettings.canEdit
                                       && root.windowsSettings.draftDirty
                            accessibleDescription: qsTr("Discard unsaved changes")
                            onClicked: root.windowsSettings.revertDraft()
                        }

                        Button {
                            id: retryButton
                            objectName: "windowsRetryButton"
                            text: qsTr("Retry")
                            emphasized: false
                            visible: root.windowsSettings.unavailable
                            available: !root.windowsSettings.saving
                            accessibleDescription: qsTr("Reconnect to the settings service")
                            onClicked: root.windowsSettings.retry()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}
