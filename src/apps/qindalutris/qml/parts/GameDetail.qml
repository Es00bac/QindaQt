// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// The selected game: its cover and facts, a Play button that is honest about
// why it is disabled, and the per-game launch options. Options edit a local
// draft; nothing persists until Save emits the collected map.
Tk.Panel {
    id: detail

    property var game: ({})
    property var options: ({})
    property var displays: []
    property bool playable: false
    property string playReason: ""

    signal playRequested()
    signal optionsSaveRequested(var values)
    signal removeRequested(string gameId)

    readonly property bool isWine: game.sourceId === "wine"
    readonly property var displayModel: {
        let entries = [{ key: "", label: qsTr("Default display") }]
        for (const display of detail.displays) {
            entries.push(display)
        }
        return entries
    }

    function displayIndexFor(key) {
        for (let i = 0; i < detail.displayModel.length; ++i) {
            if (detail.displayModel[i].key === key) {
                return i
            }
        }
        return 0
    }

    function loadDraft() {
        gamemodeSwitch.checked = options.gamemode === true
        mangohudSwitch.checked = options.mangohud === true
        environmentArea.text = options.environment !== undefined ? options.environment : ""
        displayCombo.currentIndex = detail.displayIndexFor(
                    options.display !== undefined ? options.display : "")
        runnerCombo.currentIndex = options.runner === "proton" ? 1 : 0
        prefixField.text = options.prefixOverride !== undefined ? options.prefixOverride
                                                              : (game.winePrefix || "")
    }

    onOptionsChanged: loadDraft()
    onGameChanged: loadDraft()
    Component.onCompleted: loadDraft()

    Tk.Scroll {
        anchors.fill: parent
        overflowX: Tk.Scroll.Hidden

        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm
            padding: Tk.Theme.space.sm
            width: detail.width - Tk.Theme.space.sm * 2

            CoverTile {
                objectName: "detailCover"
                source: detail.game.coverUrl !== undefined ? detail.game.coverUrl : ""
                placeholderIcon: "gamepad-2"
                tooltip: detail.game.title || ""
                height: Math.round(width * 1.35)
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Heading {
                objectName: "detailTitle"
                text: detail.game.title || ""
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.KeyValue {
                key: qsTr("Source")
                value: detail.game.sourceLabel || ""
                mono: false
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.KeyValue {
                key: qsTr("Location")
                value: detail.game.installPath || qsTr("Not tracked")
                mono: false
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.KeyValue {
                key: qsTr("Install size")
                value: detail.game.sizeText || qsTr("Not tracked")
                mono: false
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Button {
                objectName: "playButton"
                text: qsTr("Play")
                iconName: "play"
                variant: "accent"
                large: true
                enabled: detail.playable
                tooltip: detail.playable ? qsTr("Launch this game")
                                         : detail.playReason
                onClicked: detail.playRequested()
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Caption {
                objectName: "playReasonLabel"
                visible: !detail.playable && detail.playReason.length > 0
                text: detail.playReason
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }

            Tk.SectionHeader {
                title: qsTr("Launch options")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Caption {
                text: qsTr("Display")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.ComboBox {
                id: displayCombo
                objectName: "displayCombo"
                model: detail.displayModel
                textRole: "label"
                tooltip: qsTr("Which display this game opens on")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Flex {
                direction: Tk.Flex.Row
                align: Tk.Flex.Center
                gap: Tk.Theme.space.sm
                Tk.Caption {
                    text: qsTr("gamemode")
                    Tk.Flex.grow: 1
                }
                Tk.Switch {
                    id: gamemodeSwitch
                    objectName: "gamemodeSwitch"
                    tooltip: qsTr("Run through gamemoderun when installed")
                }
            }
            Tk.Flex {
                direction: Tk.Flex.Row
                align: Tk.Flex.Center
                gap: Tk.Theme.space.sm
                Tk.Caption {
                    text: qsTr("MangoHud")
                    Tk.Flex.grow: 1
                }
                Tk.Switch {
                    id: mangohudSwitch
                    objectName: "mangohudSwitch"
                    tooltip: qsTr("Show the MangoHud performance overlay")
                }
            }
            Tk.Caption {
                text: qsTr("Extra environment (KEY=VALUE, one per line)")
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.TextArea {
                id: environmentArea
                objectName: "environmentArea"
                rows: 3
                mono: true
                placeholderText: "DXVK_HUD=compiler"
                tooltip: qsTr("Invalid lines are ignored on save")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }

            // Wine-only: runner and prefix.
            Tk.Caption {
                visible: detail.isWine
                text: qsTr("Runner")
            }
            Tk.ComboBox {
                id: runnerCombo
                objectName: "runnerCombo"
                visible: detail.isWine
                model: [{ key: "wine", label: qsTr("Wine") },
                        { key: "proton", label: qsTr("Proton") }]
                textRole: "label"
                tooltip: qsTr("What runs this Windows game")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Caption {
                visible: detail.isWine
                text: qsTr("Wine prefix")
            }
            Tk.TextField {
                id: prefixField
                objectName: "prefixField"
                visible: detail.isWine
                mono: true
                placeholderText: qsTr("~/.wine")
                tooltip: qsTr("The prefix directory this game runs in")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }

            Tk.Button {
                objectName: "saveOptionsButton"
                text: qsTr("Save options")
                iconName: "check"
                onClicked: {
                    const values = {
                        gamemode: gamemodeSwitch.checked,
                        mangohud: mangohudSwitch.checked,
                        display: detail.displayModel[displayCombo.currentIndex].key,
                        environment: environmentArea.text,
                        runner: detail.isWine
                                ? (runnerCombo.currentIndex === 1 ? "proton" : "wine")
                                : "",
                        prefixOverride: detail.isWine ? prefixField.text : "",
                    }
                    detail.optionsSaveRequested(values)
                }
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Button {
                objectName: "removeGameButton"
                visible: detail.isWine
                text: qsTr("Remove from library")
                iconName: "trash-2"
                variant: "danger"
                tooltip: qsTr("Forget this hand-added game; its files stay put")
                onClicked: detail.removeRequested(detail.game.id)
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
    }
}
