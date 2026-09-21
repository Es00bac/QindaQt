// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The Voice route: the desktop's two voice preferences, the attached
// provider's live state, which provider is in use, and a way to try it.
//
// AGENT-CONTRACT: `voiceSettings` is the route model. Every enablement here
// mirrors a model predicate; this file must not decide on its own that a
// control is usable, because the model is the half that can actually refuse.
T.Page {
    id: root

    required property var voiceSettings
    signal closeRequested()

    // AGENT-GUARD: focus entry must nominate an admitted control; the window
    // chrome owns closing, so the page does not duplicate it.
    readonly property Item firstFocusTarget: voiceInputSwitch.enabled
                                             ? voiceInputSwitch
                                             : dictateButton.enabled ? dictateButton : root
    readonly property bool compact: width < 560

    title: qsTr("Voice")
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
            objectName: "voicePageHeading"
            Layout.fillWidth: true
            text: qsTr("Voice")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "voiceFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Voice settings scroll position")
            }

            FormSurface {
                id: formSurface
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["4"]

                    StateCard {
                        objectName: "voicePrivacyNotice"
                        Layout.fillWidth: true
                        status: StateCard.Information
                        title: qsTr("What dictation sends")
                        message: qsTr("Dictation records your microphone while you hold the shortcut and hands the audio to the speech provider you choose below. A cloud provider sends that audio off this machine; a local provider does not. QindaQt itself never stores your audio or your transcripts.")
                    }

                    GridLayout {
                        objectName: "voiceStateGrid"
                        Layout.fillWidth: true
                        columns: root.compact ? 1 : 2
                        columnSpacing: Tokens.space["3"]
                        rowSpacing: Tokens.space["3"]

                        StateCard {
                            objectName: "voicePreferenceState"
                            Layout.fillWidth: true
                            status: root.voiceSettings.preferenceLoading
                                    || root.voiceSettings.preferenceSaving
                                    ? StateCard.Busy
                                    : root.voiceSettings.preferenceUnavailable
                                      ? StateCard.Error : StateCard.Success
                            title: qsTr("Voice preferences")
                            message: root.voiceSettings.preferenceStatusText.length > 0
                                ? root.voiceSettings.preferenceStatusText
                                : qsTr("The saved preferences are current.")
                            actionText: root.voiceSettings.preferenceUnavailable
                                        ? qsTr("Try again") : ""
                            onActionTriggered: root.voiceSettings.retryPreference()
                        }

                        StateCard {
                            objectName: "voiceServiceState"
                            Layout.fillWidth: true
                            status: root.voiceSettings.serviceAvailable
                                    ? StateCard.Success : StateCard.Error
                            title: root.voiceSettings.serviceAvailable
                                   ? qsTr("Voice provider connected")
                                   : qsTr("No voice provider")
                            message: root.voiceSettings.serviceAvailable
                                     ? qsTr("%1 · %2")
                                        .arg(root.voiceSettings.providerLabel)
                                        .arg(root.voiceSettings.sessionStateText)
                                     : root.voiceSettings.serviceStatusText
                            actionText: root.voiceSettings.serviceAvailable
                                        ? "" : qsTr("Retry")
                            onActionTriggered: root.voiceSettings.retryProvider()
                        }
                    }

                    FormRow {
                        objectName: "voiceInputPreferenceRow"
                        Layout.fillWidth: true
                        label: qsTr("Voice input")
                        description: qsTr("Let QindaQt start and use a speech provider for dictation. Off by default.")
                        errorMessage: root.voiceSettings.preferenceErrorText
                        editor: voiceInputSwitch

                        Switch {
                            id: voiceInputSwitch
                            objectName: "voiceInputSwitch"
                            text: root.voiceSettings.draftVoiceInputEnabled
                                  ? qsTr("On") : qsTr("Off")
                            checked: root.voiceSettings.draftVoiceInputEnabled
                            enabled: root.voiceSettings.canEditPreference
                            accessibleDescription: qsTr("When on, the panel applet connects to a speech provider and the dictation shortcuts work in every window.")
                            onToggled: {
                                if (root.voiceSettings.setDraftVoiceInputEnabled(checked))
                                    root.voiceSettings.applyPreferences()
                            }
                        }
                    }

                    FormRow {
                        objectName: "voicePanelTranscriptRow"
                        Layout.fillWidth: true
                        label: qsTr("Show what you are saying in the panel")
                        description: qsTr("While you dictate, the panel chip shows the words as they are recognised. Turn this off if the panel is visible to other people.")
                        editor: panelTranscriptSwitch

                        Switch {
                            id: panelTranscriptSwitch
                            objectName: "voicePanelTranscriptSwitch"
                            text: root.voiceSettings.draftPanelTranscriptEnabled
                                  ? qsTr("On") : qsTr("Off")
                            checked: root.voiceSettings.draftPanelTranscriptEnabled
                            enabled: root.voiceSettings.canEditPreference
                            accessibleDescription: qsTr("Controls whether live partial transcripts appear on the panel while you dictate.")
                            onToggled: {
                                if (root.voiceSettings.setDraftPanelTranscriptEnabled(checked))
                                    root.voiceSettings.applyPreferences()
                            }
                        }
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Speech provider")
                        description: root.voiceSettings.canChooseProvider
                            ? qsTr("Choose which provider transcribes your speech.")
                            : qsTr("The attached provider decides how speech is transcribed.")
                    }

                    FormRow {
                        objectName: "voiceProviderRow"
                        Layout.fillWidth: true
                        visible: root.voiceSettings.providerRows.length > 0
                        label: qsTr("Provider")
                        description: qsTr("A provider marked unavailable is installed but not configured yet.")
                        editor: providerBox

                        ComboBox {
                            id: providerBox
                            objectName: "voiceProviderCombo"
                            enabled: root.voiceSettings.canChooseProvider
                            model: root.voiceSettings.providerRows
                            textRole: "label"
                            valueRole: "providerId"
                            currentIndex: {
                                const rows = root.voiceSettings.providerRows
                                for (let i = 0; i < rows.length; ++i) {
                                    if (rows[i].current === true)
                                        return i
                                }
                                return -1
                            }
                            // AGENT-GUARD: the combo reflects the provider, not
                            // the click. It is re-bound after every activation
                            // so a refused switch snaps back instead of
                            // claiming a provider that is not in use.
                            onActivated: index => {
                                const rows = root.voiceSettings.providerRows
                                if (index >= 0 && index < rows.length)
                                    root.voiceSettings.selectProvider(rows[index].providerId)
                                currentIndex = Qt.binding(() => {
                                    const current = root.voiceSettings.providerRows
                                    for (let i = 0; i < current.length; ++i) {
                                        if (current[i].current === true)
                                            return i
                                    }
                                    return -1
                                })
                            }
                        }
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                        title: qsTr("What this provider can do")
                    }

                    Repeater {
                        objectName: "voiceCapabilityRows"
                        model: root.voiceSettings.serviceAvailable
                               ? root.voiceSettings.capabilityRows : []

                        Label {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.supported === true
                                  ? qsTr("%1: supported").arg(modelData.label)
                                  : qsTr("%1: not supported").arg(modelData.label)
                            muted: modelData.supported !== true
                            Accessible.name: text
                        }
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                        title: qsTr("Shortcuts and devices")
                    }

                    Label {
                        objectName: "voiceDictationShortcut"
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                        text: qsTr("Dictation shortcut: %1")
                            .arg(root.voiceSettings.dictationShortcut.length > 0
                                 ? root.voiceSettings.dictationShortcut : qsTr("not set"))
                        Accessible.name: text
                    }

                    Label {
                        objectName: "voiceCommandShortcut"
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                                 && root.voiceSettings.commandShortcut.length > 0
                        text: qsTr("Command shortcut: %1")
                            .arg(root.voiceSettings.commandShortcut)
                        Accessible.name: text
                    }

                    Label {
                        objectName: "voiceMicrophone"
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                                 && root.voiceSettings.microphoneLabel.length > 0
                        text: qsTr("Microphone: %1").arg(root.voiceSettings.microphoneLabel)
                        Accessible.name: text
                    }

                    Label {
                        objectName: "voiceLanguage"
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                                 && root.voiceSettings.languageLabel.length > 0
                        text: qsTr("Language: %1").arg(root.voiceSettings.languageLabel)
                        Accessible.name: text
                    }

                    Label {
                        objectName: "voiceLastRoute"
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                                 && root.voiceSettings.routeLabel.length > 0
                        text: qsTr("Last insertion used: %1").arg(root.voiceSettings.routeLabel)
                        muted: true
                        Accessible.name: text
                    }

                    FormRow {
                        objectName: "voiceShortcutArmRow"
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                        label: qsTr("Dictation shortcuts armed")
                        description: qsTr("Disarm to keep the provider running without the microphone responding to its shortcuts.")
                        editor: armedSwitch

                        Switch {
                            id: armedSwitch
                            objectName: "voiceArmedSwitch"
                            text: root.voiceSettings.shortcutsArmed ? qsTr("Armed")
                                                                    : qsTr("Disarmed")
                            checked: root.voiceSettings.shortcutsArmed
                            enabled: root.voiceSettings.serviceAvailable
                                     && !root.voiceSettings.providerBusy
                            accessibleDescription: qsTr("Arms or disarms the provider's push-to-talk shortcuts.")
                            onToggled: {
                                root.voiceSettings.setShortcutsArmed(checked)
                                checked = Qt.binding(() => root.voiceSettings.shortcutsArmed)
                            }
                        }
                    }

                    Label {
                        objectName: "voiceProviderError"
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.voiceSettings.providerErrorText
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                        title: qsTr("Try it")
                        description: qsTr("Dictation inserts into whatever window has keyboard focus — including this one.")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                        spacing: Tokens.space["2"]

                        Button {
                            id: dictateButton
                            objectName: "voiceStartDictationButton"
                            text: qsTr("Start dictating")
                            available: root.voiceSettings.canStartDictation
                            busy: root.voiceSettings.providerBusy
                            accessibleDescription: qsTr("Opens the microphone and inserts what you say into the focused window.")
                            onClicked: root.voiceSettings.startDictation()
                        }

                        Button {
                            objectName: "voiceCancelDictationButton"
                            text: qsTr("Cancel")
                            emphasized: false
                            available: root.voiceSettings.canCancelDictation
                            accessibleDescription: qsTr("Stops the current recording and discards it.")
                            onClicked: root.voiceSettings.cancelDictation()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Label {
                        objectName: "voiceLastText"
                        Layout.fillWidth: true
                        visible: root.voiceSettings.serviceAvailable
                                 && root.voiceSettings.lastText.length > 0
                        text: qsTr("Last dictation: %1").arg(root.voiceSettings.lastText)
                        Accessible.name: text
                    }
                }
            }
        }
    }
}
