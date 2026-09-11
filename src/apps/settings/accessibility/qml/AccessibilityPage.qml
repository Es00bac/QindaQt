// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var accessibilitySettings
    signal closeRequested()

    // AGENT-GUARD: Focus entry must nominate an admitted domain control; the
    // window chrome owns closing, so the page does not duplicate it.
    readonly property Item firstFocusTarget: highContrastSwitch.enabled ? highContrastSwitch
                                             : retryButton.visible ? retryButton
                                                                   : root
    readonly property bool compact: width < 560
    // AGENT-NOTE: Explanations live in tooltips and accessible descriptions,
    // not in visible paragraphs (product direction: short labels, minimal
    // prose). Keep each description one sentence.
    readonly property string highContrastHelp: qsTr("Stronger outlines and solid surfaces for readability.")
    readonly property string reducedMotionHelp: qsTr("Shortens or removes interface animations.")
    readonly property string reducedTransparencyHelp: qsTr("Replaces translucent panels with solid ones.")
    readonly property string textScaleHelp: qsTr("Scales interface text. %1× is the default.")
        .arg(root.accessibilitySettings.defaultTextScale)

    title: qsTr("Accessibility")
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
            objectName: "accessibilityPageHeading"
            Layout.fillWidth: true
            text: qsTr("Accessibility")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "accessibilityFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Accessibility settings scroll position")
            }

            FormSurface {
                id: formSurface
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["3"]

                    DegradedNotice {
                        objectName: "accessibilityDegradedNotice"
                        Layout.fillWidth: true
                        visible: root.accessibilitySettings.unavailable
                        reason: root.accessibilitySettings.statusText
                        retryText: qsTr("Retry")
                        onRetryRequested: root.accessibilitySettings.retry()
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Display")
                    }

                    Switch {
                        id: highContrastSwitch
                        objectName: "accessibilityHighContrastSwitch"
                        Layout.fillWidth: true
                        text: qsTr("High contrast")
                        checked: root.accessibilitySettings.draftHighContrast
                        enabled: root.accessibilitySettings.canEdit
                        accessibleDescription: root.highContrastHelp
                        T.ToolTip.text: root.highContrastHelp
                        T.ToolTip.visible: hovered
                        T.ToolTip.delay: 500
                        onToggled: root.accessibilitySettings.setDraftHighContrast(checked)
                    }

                    Switch {
                        id: reducedMotionSwitch
                        objectName: "accessibilityReducedMotionSwitch"
                        Layout.fillWidth: true
                        text: qsTr("Reduce motion")
                        checked: root.accessibilitySettings.draftReducedMotion
                        enabled: root.accessibilitySettings.canEdit
                        accessibleDescription: root.reducedMotionHelp
                        T.ToolTip.text: root.reducedMotionHelp
                        T.ToolTip.visible: hovered
                        T.ToolTip.delay: 500
                        onToggled: root.accessibilitySettings.setDraftReducedMotion(checked)
                    }

                    Switch {
                        id: reducedTransparencySwitch
                        objectName: "accessibilityReducedTransparencySwitch"
                        Layout.fillWidth: true
                        text: qsTr("Reduce transparency")
                        checked: root.accessibilitySettings.draftReducedTransparency
                        enabled: root.accessibilitySettings.canEdit
                        accessibleDescription: root.reducedTransparencyHelp
                        T.ToolTip.text: root.reducedTransparencyHelp
                        T.ToolTip.visible: hovered
                        T.ToolTip.delay: 500
                        onToggled: root.accessibilitySettings.setDraftReducedTransparency(checked)
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Text")
                    }

                    GridLayout {
                        objectName: "accessibilityTextScaleRow"
                        Layout.fillWidth: true
                        columns: root.compact ? 1 : 3
                        columnSpacing: Tokens.space["3"]
                        rowSpacing: Tokens.space["2"]

                        Label {
                            text: qsTr("Text scale")
                            Accessible.ignored: true
                        }

                        Slider {
                            id: textScaleSlider
                            objectName: "accessibilityTextScaleSlider"
                            Layout.fillWidth: true
                            from: root.accessibilitySettings.minimumTextScale
                            to: root.accessibilitySettings.maximumTextScale
                            stepSize: 0.05
                            enabled: root.accessibilitySettings.canEdit
                            accessibleName: qsTr("Text scale")
                            accessibleDescription: root.textScaleHelp
                            T.ToolTip.text: root.textScaleHelp
                            T.ToolTip.visible: hovered
                            T.ToolTip.delay: 500
                            // Only an interactive move writes the draft; a
                            // confirmed refresh can never replay a value.
                            onMoved: root.accessibilitySettings.setDraftTextScale(value)
                        }

                        // AGENT-NOTE: A plain `value:` binding would break on
                        // the first drag; this element re-asserts the model's
                        // draft whenever the handle is not pressed so Revert
                        // and remote changes move the slider.
                        Binding {
                            target: textScaleSlider
                            property: "value"
                            value: root.accessibilitySettings.draftTextScale
                            when: !textScaleSlider.pressed
                            restoreMode: Binding.RestoreBindingOrValue
                        }

                        Label {
                            objectName: "accessibilityTextScaleValue"
                            text: qsTr("%1×").arg(textScaleSlider.value.toFixed(2))
                            muted: true
                            Accessible.ignored: true
                        }
                    }

                    Label {
                        objectName: "accessibilityTextScaleSample"
                        Layout.fillWidth: true
                        text: qsTr("Sample text")
                        // The slider reports 0 until its range binds, and a
                        // non-positive point size is a fatal QML warning.
                        font.pointSize: Tokens.type.body * Math.max(
                            root.accessibilitySettings.minimumTextScale,
                            textScaleSlider.value)
                        Accessible.name: qsTr("Sample text at %1×")
                            .arg(textScaleSlider.value.toFixed(2))
                    }

                    Label {
                        objectName: "accessibilityTextScaleError"
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.accessibilitySettings.textScaleError
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    Label {
                        objectName: "accessibilityStatus"
                        Layout.fillWidth: true
                        visible: text.length > 0 && !root.accessibilitySettings.unavailable
                        text: root.accessibilitySettings.statusText
                        muted: true
                        Accessible.role: root.accessibilitySettings.conflict
                                         ? Accessible.AlertMessage
                                         : Accessible.StaticText
                        Accessible.name: text
                    }

                    Label {
                        objectName: "accessibilityError"
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.accessibilitySettings.errorText
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    RowLayout {
                        objectName: "accessibilityActionRow"
                        Layout.fillWidth: true
                        spacing: Tokens.space["3"]

                        Button {
                            id: applyButton
                            objectName: "accessibilityApplyButton"
                            text: qsTr("Apply")
                            available: root.accessibilitySettings.applyAvailable
                            busy: root.accessibilitySettings.saving
                            accessibleDescription: qsTr("Save the changed accessibility settings")
                            onClicked: root.accessibilitySettings.applyDraft()
                        }

                        Button {
                            id: revertButton
                            objectName: "accessibilityRevertButton"
                            text: qsTr("Revert")
                            emphasized: false
                            available: root.accessibilitySettings.canEdit
                                       && root.accessibilitySettings.draftDirty
                            accessibleDescription: qsTr("Discard unsaved changes")
                            onClicked: root.accessibilitySettings.revertDraft()
                        }

                        Button {
                            id: retryButton
                            objectName: "accessibilityRetryButton"
                            text: qsTr("Retry")
                            emphasized: false
                            visible: root.accessibilitySettings.unavailable
                            available: !root.accessibilitySettings.saving
                            accessibleDescription: qsTr("Reconnect to the settings service")
                            onClicked: root.accessibilitySettings.retry()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}
