// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// The verdict card (ADR-0275 section 7): what QindaQt knows about a game in
// plain language -- one verdict first, then the evidence. The map comes from
// Installs.verdictForGame / verdictForStore (compat_advice_view.h).
Tk.Card {
    id: card

    property var verdict: ({})

    readonly property bool found: verdict.found === true
    readonly property string level: verdict.verdict !== undefined ? verdict.verdict : "unknown"

    visible: card.found

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.xs
        width: parent.width

        Tk.Flex {
            direction: Tk.Flex.Row
            align: Tk.Flex.Center
            gap: Tk.Theme.space.sm
            Tk.Label {
                text: qsTr("Compatibility")
                font.weight: Font.DemiBold
                Tk.Flex.grow: 1
            }
            Tk.Badge {
                objectName: "verdictBadge"
                text: card.verdict.verdictText !== undefined ? card.verdict.verdictText : ""
                variant: card.level === "works" ? "success"
                       : card.level === "fixes" ? "info"
                       : card.level === "blocked" ? "danger" : "default"
            }
        }
        Repeater {
            model: [card.verdict.antiCheat, card.verdict.steamDeck, card.verdict.protondb]
                   .filter(line => line !== undefined && line.length > 0)
            delegate: Tk.Caption {
                required property var modelData
                text: modelData
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
        Repeater {
            model: card.verdict.fixes !== undefined ? card.verdict.fixes : []
            delegate: Tk.Caption {
                required property var modelData
                text: "• " + modelData
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
        Repeater {
            model: card.verdict.avoid !== undefined ? card.verdict.avoid : []
            delegate: Tk.Caption {
                required property var modelData
                text: qsTr("Avoid %1: %2").arg(modelData.build).arg(modelData.reason)
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
        Tk.Caption {
            objectName: "verdictSources"
            text: card.verdict.sources !== undefined ? card.verdict.sources : ""
            wrapMode: Text.Wrap
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
    }
}
