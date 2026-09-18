// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The decoration document chooser (ADR-0207): one grid of document cards
// bound to one draft key ("appearance.windowDecoration" or
// "appearance.containerDecoration"). The route model is the only settings
// authority; this component forwards the picked id verbatim.
ColumnLayout {
    id: root

    required property var appearanceSettings
    required property bool editorBusy
    required property string settingsKey
    required property string kind
    required property string title
    property string description: ""
    property string objectPrefix: "appearanceDecorationDocument"
    readonly property var documents: appearanceSettings.decorationDocuments ?? []
    readonly property string currentValue: String(appearanceSettings.draft[settingsKey] ?? "theme")
    property Item firstChoice: null

    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        objectName: root.objectPrefix + "Header"
        title: root.title
        description: root.description
    }

    Flow {
        objectName: root.objectPrefix + "Choices"
        Layout.fillWidth: true
        Layout.preferredHeight: childrenRect.height
        Layout.minimumHeight: childrenRect.height
        spacing: Tokens.space["2"]

        Repeater {
            objectName: root.objectPrefix + "Repeater"
            model: root.documents
            onItemAdded: (index, card) => {
                if (index === 0 && root.firstChoice === null)
                    root.firstChoice = card
            }

            delegate: DecorationDocumentCard {
                id: card

                required property var modelData

                objectName: root.objectPrefix + "_" + card.modelData.id
                documentName: card.modelData.name ?? card.modelData.id
                description: card.modelData.description ?? ""
                kind: root.kind
                previewChrome: card.modelData.previewChrome ?? ({})
                previewContainerStyle: card.modelData.previewContainerStyle ?? ({})
                toolkitPalette: root.appearanceSettings.previewToolkitPalette ?? ({})
                toolkitFont: root.appearanceSettings.previewToolkitFont
                             ?? Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.body })
                canvas: root.appearanceSettings.previewCanvasColor ?? Tokens.bg.base
                wallpaper: root.appearanceSettings.previewWallpaper ?? ""
                available: root.appearanceSettings.canEdit && !root.editorBusy
                checked: root.currentValue === card.modelData.id
                onToggled: {
                    if (card.checked)
                        root.appearanceSettings.setDraftValue(root.settingsKey, card.modelData.id)
                }
            }
        }
    }
}
