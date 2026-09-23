// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import QindaTK as Tk
import QindaTK.QindaQt
import QindaQt.Voice

// The Voice console: a QindaTK window over org.qindaqt.Voice1.
//
// AGENT-CONTRACT: `VoiceConsole` is the C++ model singleton. Every enablement
// here mirrors one of its predicates; this file composes layout and never
// decides on its own that a control is usable, because the model is the half
// that can actually refuse.
Tk.AppWindow {
    id: window

    width: 980
    height: 640
    title: qsTr("Voice")

    // Feeds the desktop's QST-1 semantic tokens into Tk.Theme, so the console
    // follows the session theme and its accessibility settings.
    QindaQtTheme {}

    property int selectedEntryId: -1

    readonly property var selectedEntry: {
        const rows = VoiceConsole.historyRows
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].entryId === window.selectedEntryId)
                return rows[i]
        }
        return rows.length > 0 ? rows[0] : null
    }

    menuBar: Tk.MenuBar {
        objectName: "voiceMenuBar"

        Tk.Menu {
            title: qsTr("&File")
            Tk.MenuItem {
                text: qsTr("Clear session history")
                iconName: "trash-2"
                enabled: VoiceConsole.historyCount > 0
                onTriggered: VoiceConsole.clearHistory()
            }
            Tk.MenuSeparator {}
            Tk.MenuItem {
                text: qsTr("Close Window")
                shortcut: "Ctrl+W"
                onTriggered: window.close()
            }
        }

        Tk.Menu {
            title: qsTr("&Voice")
            Tk.MenuItem {
                text: qsTr("Start dictating")
                iconName: "mic"
                enabled: VoiceConsole.canDictate
                onTriggered: VoiceConsole.startDictation()
            }
            Tk.MenuItem {
                text: qsTr("Start a command")
                iconName: "terminal"
                enabled: VoiceConsole.canCommand
                onTriggered: VoiceConsole.startCommand()
            }
            Tk.MenuItem {
                text: qsTr("Finish")
                iconName: "check"
                enabled: VoiceConsole.canFinish
                onTriggered: VoiceConsole.finish()
            }
            Tk.MenuItem {
                text: qsTr("Cancel recording")
                iconName: "x"
                enabled: VoiceConsole.canFinish
                onTriggered: VoiceConsole.cancel()
            }
            Tk.MenuSeparator {}
            Tk.MenuItem {
                text: qsTr("Retry the last insertion")
                iconName: "refresh-cw"
                enabled: VoiceConsole.canRetry
                onTriggered: VoiceConsole.retry()
            }
            Tk.MenuItem {
                text: qsTr("Undo the last dictation")
                iconName: "undo-2"
                enabled: VoiceConsole.canUndo
                onTriggered: VoiceConsole.undo()
            }
            Tk.MenuItem {
                text: qsTr("Copy the last dictation")
                iconName: "copy"
                enabled: VoiceConsole.canCopy
                onTriggered: VoiceConsole.copyLast()
            }
        }
    }

    toolBars: [
        Tk.ToolBar {
            objectName: "voiceToolBar"
            wrap: true

            Tk.Button {
                objectName: "voiceDictateButton"
                text: VoiceConsole.capturing ? qsTr("Finish") : qsTr("Dictate")
                iconName: VoiceConsole.capturing ? "check" : "mic"
                variant: "accent"
                available: VoiceConsole.capturing ? VoiceConsole.canFinish
                                                  : VoiceConsole.canDictate
                busy: VoiceConsole.busy
                tooltip: VoiceConsole.dictationShortcut.length > 0
                         ? qsTr("Or hold %1 anywhere").arg(VoiceConsole.dictationShortcut)
                         : qsTr("Open the microphone")
                onClicked: VoiceConsole.capturing ? VoiceConsole.finish()
                                                  : VoiceConsole.startDictation()
            }

            Tk.Button {
                objectName: "voiceCommandButton"
                text: qsTr("Command")
                iconName: "terminal"
                available: VoiceConsole.canCommand
                tooltip: VoiceConsole.commandShortcut.length > 0
                         ? qsTr("Or hold %1 anywhere").arg(VoiceConsole.commandShortcut)
                         : qsTr("Speak a desktop command")
                onClicked: VoiceConsole.startCommand()
            }

            Tk.Button {
                objectName: "voiceCancelButton"
                text: qsTr("Cancel")
                iconName: "x"
                variant: "ghost"
                available: VoiceConsole.canFinish
                onClicked: VoiceConsole.cancel()
            }

            Tk.ToolSeparator {}

            Tk.Badge {
                objectName: "voiceStateBadge"
                text: VoiceConsole.stateLabel
                variant: VoiceConsole.stateVariant
            }

            Tk.ProgressBar {
                objectName: "voiceLevelBar"
                width: 120
                visible: VoiceConsole.capturing
                from: 0
                to: 100
                value: VoiceConsole.levelPercent
                tooltip: qsTr("Capture level")
            }

            Tk.Spacer {}

            Tk.Switch {
                objectName: "voiceArmedSwitch"
                text: VoiceConsole.shortcutsArmed ? qsTr("Shortcuts armed")
                                                  : qsTr("Shortcuts off")
                checked: VoiceConsole.shortcutsArmed
                enabled: VoiceConsole.serviceAvailable && !VoiceConsole.busy
                tooltip: qsTr("Arm or disarm push-to-talk everywhere on the desktop")
                // AGENT-GUARD: re-bound after every toggle so a refused switch
                // snaps back rather than claiming a state the provider is not in.
                onToggled: {
                    VoiceConsole.setShortcutsArmed(checked)
                    checked = Qt.binding(() => VoiceConsole.shortcutsArmed)
                }
            }
        }
    ]

    findBar: Tk.SearchField {
        objectName: "voiceHistoryFilter"
        visible: VoiceConsole.historyCount > 0
        placeholderText: qsTr("Search what you have dictated")
        onSearched: text => VoiceConsole.filterText = text
    }

    statusBar: Tk.StatusBar {
        objectName: "voiceStatusBar"

        Tk.StatusField {
            objectName: "voiceStatusProvider"
            iconName: "waveform"
            text: VoiceConsole.serviceAvailable
                  ? VoiceConsole.providerLabel : qsTr("No provider")
            muted: !VoiceConsole.serviceAvailable
        }
        Tk.StatusField {
            objectName: "voiceStatusLanguage"
            visible: VoiceConsole.languageLabel.length > 0
            iconName: "languages"
            text: VoiceConsole.languageLabel
        }
        Tk.StatusField {
            objectName: "voiceStatusMicrophone"
            visible: VoiceConsole.microphoneLabel.length > 0
            iconName: "mic"
            text: VoiceConsole.microphoneLabel
        }
        Tk.Spacer {}
        Tk.StatusField {
            objectName: "voiceStatusRoute"
            visible: VoiceConsole.routeLabel.length > 0
            iconName: "corner-down-right"
            text: qsTr("via %1").arg(VoiceConsole.routeLabel)
            muted: true
        }
        Tk.StatusField {
            objectName: "voiceStatusCount"
            iconName: "list"
            text: qsTr("%n in this session", "", VoiceConsole.historyCount)
            muted: true
        }
    }

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        gap: 0

        Tk.Notice {
            objectName: "voiceServiceNotice"
            Tk.Flex.grow: 0
            width: parent.width
            visible: VoiceConsole.statusText.length > 0
            variant: "warning"
            title: qsTr("Voice input is not connected")
            text: VoiceConsole.statusText

            actions: Tk.Button {
                text: qsTr("Try again")
                visible: VoiceConsole.canRetryConnection
                small: true
                variant: "outline"
                onClicked: VoiceConsole.retryConnection()
            }
        }

        Tk.Notice {
            objectName: "voiceFeedbackNotice"
            Tk.Flex.grow: 0
            width: parent.width
            visible: VoiceConsole.feedback.length > 0
            variant: VoiceConsole.feedbackVariant
            text: VoiceConsole.feedback
            dismissible: true
            onDismissed: VoiceConsole.dismissFeedback()
        }

        Tk.Box {
            objectName: "voiceLiveBand"
            Tk.Flex.grow: 0
            width: parent.width
            visible: VoiceConsole.capturing
            padding: Tk.Theme.space.md
            color: Tk.Theme.color.accentSubtle
            borderBottom: 1
            borderColor: Tk.Theme.color.border

            Tk.Label {
                width: parent.width
                accent: true
                text: VoiceConsole.partialText.length > 0
                      ? VoiceConsole.partialText
                      : qsTr("Listening…")
            }
        }

        Tk.Splitter {
            Tk.Flex.grow: 1
            width: parent.width
            orientation: Qt.Horizontal

            Tk.Panel {
                objectName: "voiceHistoryPanel"
                T.SplitView.preferredWidth: 420
                T.SplitView.minimumWidth: 260
                title: qsTr("This session")
                iconName: "history"

                actions: Tk.IconButton {
                    iconName: "trash-2"
                    small: true
                    tooltip: qsTr("Clear the session history")
                    enabled: VoiceConsole.historyCount > 0
                    onClicked: VoiceConsole.clearHistory()
                }

                Tk.EmptyState {
                    anchors.fill: parent
                    visible: VoiceConsole.historyRows.length === 0
                    iconName: "mic-off"
                    title: VoiceConsole.historyCount === 0
                           ? qsTr("Nothing dictated yet")
                           : qsTr("No matches")
                    text: VoiceConsole.historyCount === 0
                          ? qsTr("Hold the dictation shortcut in any window — a terminal, a browser, a chat box — and what you say is inserted there. It also shows up here.")
                          : qsTr("No dictation in this session contains that text.")
                }

                Tk.Scroll {
                    anchors.fill: parent
                    visible: VoiceConsole.historyRows.length > 0

                    Tk.Flex {
                        width: parent.width
                        direction: Tk.Flex.Column

                        Repeater {
                            model: VoiceConsole.historyRows

                            Tk.ListRow {
                                required property var modelData

                                width: parent.width
                                text: modelData.text
                                secondaryText: modelData.command
                                    ? qsTr("%1 · command").arg(modelData.timeText)
                                    : modelData.routeLabel.length > 0
                                      ? qsTr("%1 · %2").arg(modelData.timeText)
                                                       .arg(modelData.routeLabel)
                                      : modelData.timeText
                                iconName: modelData.command ? "terminal" : "mic"
                                selected: modelData.entryId === window.selectedEntryId
                                onClicked: window.selectedEntryId = modelData.entryId
                                onDoubleClicked: VoiceConsole.copyHistoryEntry(modelData.entryId)
                            }
                        }
                    }
                }
            }

            Tk.Panel {
                objectName: "voiceDetailPanel"
                T.SplitView.fillWidth: true
                title: qsTr("Details")
                iconName: "info"

                Tk.Scroll {
                    anchors.fill: parent

                    Tk.Flex {
                        width: parent.width
                        direction: Tk.Flex.Column
                        gap: Tk.Theme.space.md

                        Tk.PropertyGroup {
                            width: parent.width
                            title: qsTr("Selected dictation")
                            visible: window.selectedEntry !== null

                            Tk.Label {
                                objectName: "voiceSelectedText"
                                width: parent.width
                                selectable: true
                                wrapMode: Text.Wrap
                                elide: Text.ElideNone
                                text: window.selectedEntry
                                      ? window.selectedEntry.text : ""
                            }

                            Tk.KeyValue {
                                width: parent.width
                                key: qsTr("Spoken at")
                                value: window.selectedEntry
                                       ? window.selectedEntry.timeText : ""
                            }

                            Tk.KeyValue {
                                width: parent.width
                                visible: window.selectedEntry
                                         && window.selectedEntry.routeLabel.length > 0
                                key: qsTr("Inserted through")
                                value: window.selectedEntry
                                       ? window.selectedEntry.routeLabel : ""
                            }

                            Tk.Button {
                                objectName: "voiceCopySelected"
                                text: qsTr("Copy")
                                iconName: "copy"
                                small: true
                                available: window.selectedEntry !== null
                                onClicked: {
                                    if (window.selectedEntry)
                                        VoiceConsole.copyHistoryEntry(window.selectedEntry.entryId)
                                }
                            }
                        }

                        Tk.PropertyGroup {
                            width: parent.width
                            title: qsTr("Provider")

                            Tk.PropertyRow {
                                width: parent.width
                                label: qsTr("Speech")
                                hint: VoiceConsole.canChooseProvider
                                      ? qsTr("Applies to your next dictation.")
                                      : qsTr("Set by the attached provider.")

                                Tk.ComboBox {
                                    objectName: "voiceProviderCombo"
                                    enabled: VoiceConsole.canChooseProvider
                                    model: VoiceConsole.providerRows
                                    textRole: "label"
                                    currentIndex: {
                                        const rows = VoiceConsole.providerRows
                                        for (let i = 0; i < rows.length; ++i) {
                                            if (rows[i].current === true)
                                                return i
                                        }
                                        return -1
                                    }
                                    onActivated: index => {
                                        const rows = VoiceConsole.providerRows
                                        if (index >= 0 && index < rows.length)
                                            VoiceConsole.selectProvider(rows[index].providerId)
                                        currentIndex = Qt.binding(() => {
                                            const now = VoiceConsole.providerRows
                                            for (let i = 0; i < now.length; ++i) {
                                                if (now[i].current === true)
                                                    return i
                                            }
                                            return -1
                                        })
                                    }
                                }
                            }

                            Tk.KeyValue {
                                width: parent.width
                                visible: VoiceConsole.dictationShortcut.length > 0
                                key: qsTr("Dictation shortcut")
                                value: VoiceConsole.dictationShortcut
                                mono: true
                            }

                            Tk.KeyValue {
                                width: parent.width
                                visible: VoiceConsole.commandShortcut.length > 0
                                key: qsTr("Command shortcut")
                                value: VoiceConsole.commandShortcut
                                mono: true
                            }

                            Tk.KeyValue {
                                width: parent.width
                                visible: VoiceConsole.microphoneLabel.length > 0
                                key: qsTr("Microphone")
                                value: VoiceConsole.microphoneLabel
                            }
                        }

                        Tk.PropertyGroup {
                            width: parent.width
                            title: qsTr("What this provider can do")
                            visible: VoiceConsole.serviceAvailable

                            Repeater {
                                model: VoiceConsole.capabilityRows

                                Tk.KeyValue {
                                    required property var modelData

                                    width: parent.width
                                    key: modelData.label
                                    value: modelData.supported === true
                                           ? qsTr("Yes") : qsTr("No")
                                    accent: modelData.supported === true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
