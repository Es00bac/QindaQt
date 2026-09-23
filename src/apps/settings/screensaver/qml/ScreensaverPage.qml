// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The Screen saver route (ADR-0226). Two separate truths, deliberately side by
// side and never merged: what an idle screen shows (the screensaverSettings
// model over Settings1), and whether the session locks itself (the shared
// screenLockSettings model over kscreenlockerrc [Daemon]). The saver list is
// discovered from the installed packages, so the selector model comes from
// the C++ model, never a hard-coded QML list.
T.Page {
    id: root

    required property var screensaverSettings
    required property var screenLockSettings
    signal closeRequested()

    readonly property Item firstFocusTarget: saverSelector.enabled ? saverSelector : root

    // The delay ladder offered for "start after". A stored out-of-ladder value
    // is kept as an extra entry so opening the page never silently changes it.
    readonly property var delayOptions: {
        const minutes = [1, 2, 5, 10, 15, 30, 60]
        const current = root.screensaverSettings.minutes
        if (root.screensaverSettings.hasConfirmed && minutes.indexOf(current) < 0) {
            minutes.push(current)
            minutes.sort(function(left, right) { return left - right })
        }
        return minutes.map(function(value) {
            return { "value": value, "label": qsTr("%1 minutes").arg(value) }
        })
    }
    // Same ladder rule for the walk-away lock timeout. The two ladders are
    // separate preferences: the saver delay is not the lock timeout.
    readonly property var lockTimeoutOptions: {
        const minutes = [1, 2, 5, 10, 15, 30, 60, 120, 240]
        const current = root.screenLockSettings.timeoutMinutes
        if (minutes.indexOf(current) < 0) {
            minutes.push(current)
            minutes.sort(function(left, right) { return left - right })
        }
        return minutes.map(function(value) {
            return { "value": value, "label": qsTr("%1 minutes").arg(value) }
        })
    }

    title: qsTr("Screen saver")
    background: Rectangle { color: Tokens.bg.base }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "screensaverPageHeading"
            Layout.fillWidth: true
            text: qsTr("Screen saver")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "screensaverFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: sections.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Screen saver settings scroll position")
            }

            ColumnLayout {
                id: sections
                width: viewport.width
                spacing: Tokens.space["4"]

                // -- What an idle screen shows --------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]
                    Accessible.role: Accessible.Grouping
                    Accessible.name: qsTr("Screen saver")

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Screen saver")
                        description: qsTr("Choose what the screen shows when the session goes idle")
                    }

                    FormSurface {
                        Layout.fillWidth: true
                        padding: Tokens.space["3"]

                        contentItem: ColumnLayout {
                            spacing: Tokens.space["2"]

                            RowLayout {
                                Layout.fillWidth: true
                                enabled: root.screensaverSettings.canEdit
                                spacing: Tokens.space["2"]

                                Label {
                                    text: qsTr("Show")
                                    Accessible.name: text
                                    muted: !parent.enabled
                                }
                                ComboBox {
                                    id: saverSelector
                                    objectName: "screensaverSaverSelector"
                                    Layout.fillWidth: true
                                    enabled: parent.enabled
                                    model: root.screensaverSettings.saverOptions
                                    textRole: "name"
                                    valueRole: "token"
                                    readonly property int confirmedIndex: root.screensaverSettings.hasConfirmed
                                        ? root.screensaverSettings.saverOptions.findIndex(function(option) {
                                            return option.token === root.screensaverSettings.saver
                                        }) : -1
                                    currentIndex: confirmedIndex
                                    accessibleDescription: qsTr("Choose the idle screensaver")
                                    // Only an interactive activation writes; a
                                    // confirmed snapshot refresh can never
                                    // replay the selection.
                                    onActivated: index => {
                                        if (index >= 0 && index < root.screensaverSettings.saverOptions.length)
                                            root.screensaverSettings.setSaver(
                                                root.screensaverSettings.saverOptions[index].token)
                                        // A keyboard choice changes currentIndex locally. Restore
                                        // the confirmed binding even if admission refuses the write.
                                        Qt.callLater(function() {
                                            saverSelector.currentIndex = Qt.binding(function() {
                                                return saverSelector.confirmedIndex
                                            })
                                        })
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                enabled: root.screensaverSettings.delayEnabled
                                         && root.screensaverSettings.canEdit
                                spacing: Tokens.space["2"]

                                Label {
                                    text: qsTr("Start after")
                                    Accessible.name: text
                                    muted: !parent.enabled
                                }
                                ComboBox {
                                    id: delaySelector
                                    objectName: "screensaverDelaySelector"
                                    Layout.fillWidth: true
                                    enabled: parent.enabled
                                    model: root.delayOptions
                                    textRole: "label"
                                    valueRole: "value"
                                    readonly property int confirmedIndex: root.screensaverSettings.hasConfirmed
                                        ? root.delayOptions.findIndex(function(option) {
                                            return option.value === root.screensaverSettings.minutes
                                        }) : -1
                                    currentIndex: confirmedIndex
                                    accessibleDescription: qsTr("Choose how long the session waits before the screensaver starts")
                                    onActivated: index => {
                                        if (index >= 0 && index < root.delayOptions.length)
                                            root.screensaverSettings.setMinutes(
                                                root.delayOptions[index].value)
                                        Qt.callLater(function() {
                                            delaySelector.currentIndex = Qt.binding(function() {
                                                return delaySelector.confirmedIndex
                                            })
                                        })
                                    }
                                }
                            }

                            Label {
                                objectName: "screensaverStatus"
                                Layout.fillWidth: true
                                text: root.screensaverSettings.statusText
                                wrapMode: Text.Wrap
                                muted: true
                                Accessible.name: text
                            }

                            Button {
                                id: previewButton
                                objectName: "screensaverPreviewButton"
                                Layout.fillWidth: true
                                visible: root.screensaverSettings.previewAvailable
                                enabled: !root.screensaverSettings.busy
                                         && !root.screensaverSettings.previewRunning
                                text: root.screensaverSettings.previewRunning
                                      ? qsTr("Preview running…") : qsTr("Preview")
                                accessibleDescription: qsTr("Show what the chosen screensaver looks like without locking the session")
                                onClicked: root.screensaverSettings.preview()
                            }

                            Label {
                                objectName: "screensaverPreviewSummary"
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: root.screensaverSettings.previewSummary
                                wrapMode: Text.Wrap
                                muted: true
                                Accessible.name: text
                            }

                            Label {
                                objectName: "screensaverNote"
                                Layout.fillWidth: true
                                text: qsTr("A screensaver does not lock the session. Any activity dismisses it, and it stops when the screen locks. Locking is set separately below.")
                                wrapMode: Text.Wrap
                                muted: true
                                Accessible.name: text
                            }

                            Label {
                                objectName: "screensaverError"
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: root.screensaverSettings.errorText
                                wrapMode: Text.Wrap
                                Accessible.role: Accessible.AlertMessage
                                Accessible.name: text
                            }
                            Button {
                                objectName: "screensaverRetry"
                                visible: root.screensaverSettings.errorText.length > 0
                                text: qsTr("Try again")
                                available: !root.screensaverSettings.busy
                                accessibleDescription: qsTr("Refresh screen saver settings without repeating a saved choice")
                                onClicked: root.screensaverSettings.retry()
                            }
                        }
                    }
                }

                // -- Whether the session locks itself --------------------------
                // A separate preference with its own model and store: the saver
                // delay above is not the lock timeout, and the UI must not
                // conflate them.
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]
                    Accessible.role: Accessible.Grouping
                    Accessible.name: qsTr("Locking")

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Locking")
                        description: qsTr("Choose whether walking away locks this session")
                    }

                    FormSurface {
                        Layout.fillWidth: true
                        padding: Tokens.space["3"]

                        contentItem: ColumnLayout {
                            spacing: Tokens.space["2"]

                            Switch {
                                id: automaticLock
                                objectName: "screensaverAutomaticScreenLock"
                                Layout.fillWidth: true
                                text: qsTr("Lock automatically when idle")
                                checked: root.screenLockSettings.automaticLock
                                enabled: !root.screenLockSettings.busy
                                accessibleDescription: checked
                                    ? qsTr("The screen locks after the selected idle time")
                                    : qsTr("Automatic idle locking is off")
                                onToggled: root.screenLockSettings.setAutomaticLock(checked)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                enabled: root.screenLockSettings.automaticLock
                                         && !root.screenLockSettings.busy
                                spacing: Tokens.space["2"]

                                Label {
                                    text: qsTr("Lock after")
                                    Accessible.name: text
                                    muted: !parent.enabled
                                }
                                ComboBox {
                                    id: lockTimeoutSelector
                                    objectName: "screensaverLockTimeoutSelector"
                                    Layout.fillWidth: true
                                    enabled: parent.enabled
                                    model: root.lockTimeoutOptions
                                    textRole: "label"
                                    valueRole: "value"
                                    currentIndex: Math.max(0, root.lockTimeoutOptions.findIndex(function(option) {
                                        return option.value === root.screenLockSettings.timeoutMinutes
                                    }))
                                    accessibleDescription: qsTr("Choose the automatic idle-lock timeout")
                                    onActivated: index => {
                                        if (index >= 0 && index < root.lockTimeoutOptions.length)
                                            root.screenLockSettings.setTimeoutMinutes(
                                                root.lockTimeoutOptions[index].value)
                                    }
                                }
                            }

                            Label {
                                objectName: "screensaverLockStatus"
                                Layout.fillWidth: true
                                text: root.screenLockSettings.statusText
                                wrapMode: Text.Wrap
                                muted: true
                                Accessible.name: text
                            }
                            Label {
                                objectName: "screensaverLockError"
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: root.screenLockSettings.errorText
                                wrapMode: Text.Wrap
                                Accessible.role: Accessible.AlertMessage
                                Accessible.name: text
                            }
                            Button {
                                objectName: "screensaverLockRetry"
                                visible: root.screenLockSettings.errorText.length > 0
                                text: qsTr("Try again")
                                available: !root.screenLockSettings.busy
                                accessibleDescription: qsTr("Retry the failed lock settings step")
                                onClicked: root.screenLockSettings.retryLiveApply()
                            }
                        }

                        Accessible.role: Accessible.Grouping
                        Accessible.name: qsTr("Automatic screen lock")
                    }
                }
            }
        }
    }
}
