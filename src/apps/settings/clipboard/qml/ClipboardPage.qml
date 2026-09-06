// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var clipboardSettings
    signal closeRequested()

    // AGENT-GUARD: Close stays enabled under every service, preference,
    // confirmation, and mutation state. Settings navigation may therefore
    // always enter this route through an admitted control.
    readonly property Item firstFocusTarget: closeButton
    readonly property bool compact: width < 560

    title: qsTr("Clipboard")
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
            objectName: "clipboardPageHeading"
            Layout.fillWidth: true
            text: qsTr("Clipboard")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "clipboardFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Clipboard settings scroll position")
            }

            FormSurface {
                id: formSurface
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["4"]

                    StateCard {
                        objectName: "clipboardPrivacyNotice"
                        Layout.fillWidth: true
                        status: StateCard.Information
                        title: qsTr("Off by default")
                        message: qsTr("When enabled, QindaQt temporarily keeps recent text and image clipboard selections in memory. History is never saved to disk.")
                    }

                    StateCard {
                        objectName: "clipboardContentBoundary"
                        Layout.fillWidth: true
                        status: StateCard.Information
                        title: qsTr("No clipboard content on this page")
                        message: qsTr("Settings shows only history state and item counts. It cannot read, preview, paste, copy, select, or delete individual clipboard items.")
                    }

                    GridLayout {
                        objectName: "clipboardStateGrid"
                        Layout.fillWidth: true
                        columns: root.compact ? 1 : 2
                        columnSpacing: Tokens.space["3"]
                        rowSpacing: Tokens.space["3"]

                        StateCard {
                            objectName: "clipboardPreferenceState"
                            Layout.fillWidth: true
                            status: root.clipboardSettings.preferenceLoading
                                    || root.clipboardSettings.preferenceSaving
                                    ? StateCard.Busy
                                    : root.clipboardSettings.preferenceConflict
                                      ? StateCard.Warning
                                      : root.clipboardSettings.preferenceUnavailable
                                        ? StateCard.Error : StateCard.Success
                            title: qsTr("History preference")
                            message: root.clipboardSettings.preferenceStatusText.length > 0
                                ? root.clipboardSettings.preferenceStatusText
                                : qsTr("The saved preference is current.")
                            actionText: root.clipboardSettings.preferenceUnavailable
                                        ? qsTr("Try again") : ""
                            onActionTriggered: root.clipboardSettings.retryPreference()
                        }

                        StateCard {
                            objectName: "clipboardServiceState"
                            Layout.fillWidth: true
                            status: root.clipboardSettings.privacyDenied
                                    ? StateCard.Warning
                                    : root.clipboardSettings.serviceAvailable
                                      ? StateCard.Success : StateCard.Error
                            title: root.clipboardSettings.privacyDenied
                                   ? qsTr("History hidden for privacy")
                                   : root.clipboardSettings.serviceAvailable
                                     ? qsTr("Clipboard service available")
                                     : qsTr("Clipboard service degraded")
                            message: root.clipboardSettings.serviceStatusText
                            actionText: root.clipboardSettings.serviceDegraded
                                        ? qsTr("Retry") : ""
                            onActionTriggered: root.clipboardSettings.retryClipboard()
                        }
                    }

                    FormRow {
                        objectName: "clipboardHistoryPreferenceRow"
                        Layout.fillWidth: true
                        label: qsTr("Clipboard history")
                        description: qsTr("Opt in to volatile history capture. Turning this off makes the resident service purge its current history.")
                        errorMessage: root.clipboardSettings.preferenceErrorText
                        editor: historySwitch

                        Switch {
                            id: historySwitch
                            objectName: "clipboardHistorySwitch"
                            text: root.clipboardSettings.draftHistoryEnabled
                                  ? qsTr("On") : qsTr("Off")
                            checked: root.clipboardSettings.draftHistoryEnabled
                            enabled: root.clipboardSettings.canEditPreference
                            accessibleDescription: qsTr("History is off by default. When on, recent text and image selections are held in memory.")
                            onToggled: {
                                if (root.clipboardSettings.setDraftHistoryEnabled(checked))
                                    root.clipboardSettings.applyPreference()
                            }
                        }
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Live history state")
                        description: qsTr("%1 of %2 history slots currently used")
                            .arg(root.clipboardSettings.entryCount)
                            .arg(root.clipboardSettings.capacity)
                    }

                    Label {
                        objectName: "clipboardClearStatus"
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.clipboardSettings.clearStatusText
                        Accessible.role: root.clipboardSettings.clearUncertain
                                         ? Accessible.AlertMessage
                                         : Accessible.StaticText
                        Accessible.name: text
                    }

                    Label {
                        objectName: "clipboardClearError"
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.clipboardSettings.clearErrorText
                        color: Tokens.fg.default
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    Button {
                        id: clearButton
                        objectName: "clipboardClearButton"
                        Layout.alignment: Qt.AlignLeft
                        text: qsTr("Clear history")
                        destructive: true
                        emphasized: false
                        available: root.clipboardSettings.clearAvailable
                        busy: root.clipboardSettings.clearBusy
                        accessibleDescription: root.clipboardSettings.entryCount > 0
                            ? qsTr("Permanently clear all %1 volatile history items after confirmation").arg(root.clipboardSettings.entryCount)
                            : qsTr("No clipboard history is present")
                        onClicked: {
                            if (root.clipboardSettings.requestClearHistory())
                                clearDialog.open()
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                muted: true
                text: root.clipboardSettings.preferenceDirty
                      ? qsTr("Saving clipboard history preference…") : ""
                Accessible.name: text
            }

            Button {
                id: closeButton
                objectName: "clipboardCloseButton"
                text: qsTr("Close")
                emphasized: false
                available: true
                busy: false
                onClicked: root.closeRequested()
            }
        }
    }

    T.Popup {
        id: clearDialog
        objectName: "clipboardClearDialog"
        signal accepted()
        signal rejected()

        parent: T.Overlay.overlay
        modal: true
        focus: true
        width: Math.min(480, root.width - 2 * Tokens.space["4"])
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        padding: Tokens.space["4"]
        closePolicy: T.Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: Tokens.space["3"]

            Label {
                Layout.fillWidth: true
                text: qsTr("Clear clipboard history?")
                font.pointSize: Tokens.type.title
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                id: clearMessage
                Layout.fillWidth: true
                text: qsTr("This permanently removes all volatile clipboard history. New selections may be captured afterward if history remains enabled.")
                wrapMode: Text.Wrap
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight

                Button {
                    text: qsTr("Cancel")
                    emphasized: false
                    available: true
                    onClicked: clearDialog.rejected()
                }

                Button {
                    text: qsTr("Clear history")
                    destructive: true
                    available: true
                    onClicked: clearDialog.accepted()
                }
            }
        }
        onAccepted: {
            root.clipboardSettings.confirmClearHistory()
            close()
        }
        onRejected: {
            root.clipboardSettings.cancelClearHistory()
            close()
        }
        onClosed: {
            if (root.clipboardSettings.clearConfirmationPending)
                root.clipboardSettings.cancelClearHistory()
        }

        Connections {
            target: root.clipboardSettings
            function onViewChanged() {
                if (clearDialog.visible
                        && !root.clipboardSettings.clearConfirmationPending)
                    clearDialog.close()
            }
        }
    }
}
