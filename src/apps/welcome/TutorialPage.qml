// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as QQ
import QindaQt.Tokens 1.0

ColumnLayout {
    id: page
    required property string eyebrow
    required property string title
    required property string summary
    required property string diagramKind
    required property var cards
    property var shortcuts: []
    property var actions: []
    property string heroSource: ""
    property var actionHandler
    spacing: Tokens.space["4"]

    Item { Layout.preferredHeight: Tokens.space["2"] }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["5"]
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]
            Text {
                Layout.fillWidth: true
                text: page.eyebrow.toUpperCase()
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                font.weight: Font.Bold
                font.letterSpacing: 1.2
                Accessible.ignored: true
            }
            Text {
                Layout.fillWidth: true
                text: page.title
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.display
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            Text {
                Layout.fillWidth: true
                text: page.summary
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                wrapMode: Text.Wrap
                lineHeight: 1.2
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }
        Rectangle {
            visible: page.heroSource.length > 0 && hero.status === Image.Ready
            Layout.preferredWidth: Math.min(260, page.width * 0.34)
            Layout.preferredHeight: 150
            radius: Tokens.radius.l
            color: Tokens.bg.raised
            clip: true
            border.width: 1
            border.color: Tokens.outline.divider
            Image { id: hero; anchors.fill: parent; source: page.heroSource; fillMode: Image.PreserveAspectCrop; asynchronous: true }
        }
    }

    TutorialDiagram { Layout.fillWidth: true; kind: page.diagramKind }

    Flow {
        visible: page.shortcuts.length > 0
        Layout.fillWidth: true
        spacing: Tokens.space["2"]
        Repeater { model: page.shortcuts; ShortcutPill { required property string modelData; text: modelData } }
    }

    Repeater {
        model: page.cards
        InfoCard {
            required property var modelData
            Layout.fillWidth: true
            title: modelData.title
            body: modelData.body
            marker: modelData.marker || ""
        }
    }

    Flow {
        visible: page.actions.length > 0
        Layout.fillWidth: true
        spacing: Tokens.space["3"]
        Repeater {
            model: page.actions
            QQ.Button {
                required property var modelData
                text: modelData.label
                emphasized: modelData.emphasized === true
                accessibleDescription: modelData.description
                onClicked: page.actionHandler(modelData.action)
            }
        }
    }

    Item { Layout.preferredHeight: Tokens.space["2"] }
}
