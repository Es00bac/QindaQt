// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// The voice panel body: what the microphone is doing, what it last heard,
// what can be done about it, and where the rest of voice input lives.
//
// AGENT-CONTRACT: `access` is the injected shell facade. Every enablement and
// every sentence comes from its projection; this file composes layout only.
ColumnLayout {
    id: panel

    required property var access

    readonly property bool ready: panel.access?.phase === "ready"
    readonly property bool controllable: panel.access?.controlAvailable === true
    readonly property string statusText: {
        if (!panel.access)
            return qsTr("Voice input is not connected")
        if (panel.access.feedbackPresent)
            return panel.access.feedback
        return panel.access.diagnostic ?? ""
    }
    readonly property bool statusIsError:
        (panel.access?.feedbackPresent ?? false)
            || panel.access?.stateId === "error"
            || panel.access?.phase === "unavailable"

    spacing: Tokens.space["2"]

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                objectName: "voiceStatusLabel"
                Layout.fillWidth: true
                text: panel.access ? panel.access.statusLabel : qsTr("Voice")
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                objectName: "voiceProviderLabel"
                Layout.fillWidth: true
                visible: panel.ready && text.length > 0
                text: {
                    if (!panel.access)
                        return ""
                    const provider = panel.access.providerLabel ?? ""
                    const language = panel.access.languageLabel ?? ""
                    if (provider.length === 0)
                        return language
                    if (language.length === 0)
                        return provider
                    return qsTr("%1 · %2").arg(provider).arg(language)
                }
                color: Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pixelSize: Tokens.type.caption
                elide: Text.ElideRight
            }
        }

        VoiceLevelMeter {
            objectName: "voicePanelMeter"
            Layout.alignment: Qt.AlignVCenter
            segments: 8
            visible: panel.access?.capturing === true
            active: panel.access?.capturing === true
            levelPercent: panel.access ? panel.access.levelPercent : 0
        }

        C.Switch {
            objectName: "voiceEnabledSwitch"
            Layout.alignment: Qt.AlignVCenter
            enabled: panel.controllable && !(panel.access?.operationPending ?? false)
            checked: panel.access?.voiceEnabled === true
            accessibleDescription: qsTr("Arm or disarm the voice input shortcuts")
            // AGENT-GUARD: the switch reflects the provider, not the click. It
            // is re-bound from the projection on every change, so a refused
            // request snaps it back instead of lying about the provider.
            onToggled: {
                if (panel.access)
                    panel.access.setVoiceEnabled(checked)
                checked = Qt.binding(() => panel.access?.voiceEnabled === true)
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Tokens.outline.divider
    }

    // What was heard. A partial is shown in the accent colour so the user can
    // tell a live guess from text that has already been inserted.
    Rectangle {
        objectName: "voiceTranscriptBlock"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(48, transcript.implicitHeight + Tokens.space["3"] * 2)
        visible: panel.ready
        radius: Tokens.radius.s
        color: Tokens.bg.base
        border.color: Tokens.outline.divider

        Text {
            id: transcript
            objectName: "voiceTranscriptText"
            anchors.fill: parent
            anchors.margins: Tokens.space["3"]
            text: {
                const value = panel.access?.transcriptText ?? ""
                if (value.length > 0)
                    return value
                return panel.access?.capturing === true
                    ? qsTr("Listening…")
                    : qsTr("Nothing dictated yet.")
            }
            color: (panel.access?.transcriptText ?? "").length === 0
                   ? Tokens.fg.muted
                   : panel.access?.transcriptIsPartial === true
                     ? Tokens.accent.default : Tokens.fg.default
            font.family: Tokens.type.fontFamily
            font.pixelSize: Tokens.type.caption
            wrapMode: Text.Wrap
            maximumLineCount: 4
            elide: Text.ElideRight
            Accessible.role: Accessible.StaticText
            Accessible.name: panel.access?.transcriptIsPartial === true
                             ? qsTr("Partial transcript") : qsTr("Last dictation")
            Accessible.description: text
        }
    }

    Text {
        objectName: "voiceRouteLabel"
        Layout.fillWidth: true
        visible: panel.ready && (panel.access?.routeLabel ?? "").length > 0
                 && panel.access?.capturing !== true
        text: qsTr("Inserted through: %1").arg(panel.access?.routeLabel ?? "")
        color: Tokens.fg.muted
        font.family: Tokens.type.fontFamily
        font.pixelSize: Tokens.type.caption
        elide: Text.ElideRight
    }

    Flow {
        objectName: "voiceActionFlow"
        Layout.fillWidth: true
        visible: panel.ready
        spacing: Tokens.space["1"]

        Repeater {
            model: panel.access ? panel.access.actionRows : []

            VoiceActionButton {
                required property var modelData

                actionId: modelData.actionId
                text: modelData.label
                iconName: modelData.iconName
                actionEnabled: modelData.enabled === true
                                && !(panel.access?.operationPending ?? false)
                emphasized: modelData.actionId === "dictate"
                            || modelData.actionId === "finish"
                onActivated: id => {
                    if (panel.access)
                        panel.access.invokeAction(id)
                }
            }
        }
    }

    Text {
        objectName: "voiceShortcutHint"
        Layout.fillWidth: true
        visible: panel.ready && (panel.access?.dictationShortcut ?? "").length > 0
        text: {
            const dictate = panel.access?.dictationShortcut ?? ""
            const command = panel.access?.commandShortcut ?? ""
            if (command.length === 0)
                return qsTr("Hold %1 to dictate.").arg(dictate)
            return qsTr("Hold %1 to dictate, %2 for a command.").arg(dictate).arg(command)
        }
        color: Tokens.fg.muted
        font.family: Tokens.type.fontFamily
        font.pixelSize: Tokens.type.caption
        wrapMode: Text.Wrap
    }

    Text {
        objectName: "voiceMicrophoneLabel"
        Layout.fillWidth: true
        visible: panel.ready && (panel.access?.microphoneLabel ?? "").length > 0
        text: qsTr("Microphone: %1").arg(panel.access?.microphoneLabel ?? "")
        color: Tokens.fg.muted
        font.family: Tokens.type.fontFamily
        font.pixelSize: Tokens.type.caption
        elide: Text.ElideRight
    }

    // One status line, not a card: the popup is small and a warning card
    // pushes the actions below the fold. Empty means nothing is wrong.
    RowLayout {
        Layout.fillWidth: true
        visible: panel.statusText.length > 0
        spacing: Tokens.space["2"]

        Text {
            objectName: "voiceStatusText"
            Layout.fillWidth: true
            text: panel.statusText
            color: panel.statusIsError ? Tokens.danger.default : Tokens.fg.muted
            font.family: Tokens.type.fontFamily
            font.pixelSize: Tokens.type.caption
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        VoiceActionButton {
            objectName: "voiceDismissFeedback"
            visible: panel.access?.feedbackPresent === true
            actionId: "dismiss"
            text: qsTr("Dismiss")
            iconName: "window-close"
            actionEnabled: !(panel.access?.operationPending ?? false)
            onActivated: {
                if (panel.access)
                    panel.access.dismissFeedback()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Item { Layout.fillWidth: true }

        C.Button {
            objectName: "voiceOpenConsoleButton"
            text: qsTr("Open Voice")
            emphasized: false
            available: panel.access?.canOpenConsole === true
            accessibleDescription: qsTr("Open the Voice application for history, "
                                        + "commands and vocabulary")
            onClicked: if (panel.access) panel.access.openConsole()
        }

        C.Button {
            objectName: "voiceSettingsButton"
            text: qsTr("Voice settings")
            emphasized: false
            available: panel.access?.canOpenSettings === true
            accessibleDescription: qsTr("Open the voice page in Settings")
            onClicked: if (panel.access) panel.access.openSettings()
        }
    }
}
