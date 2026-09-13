// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// KWin decoration discovery and selection is independent from Settings1's
// QindaQt chrome preferences (ADR-0160). Keep this platform-owned choice in
// one presentation component so the surrounding section only decides which
// renderer-specific controls are applicable.
ColumnLayout {
    id: root

    property var settings: null
    required property bool editorBusy
    property Item firstChoice: null

    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    SectionHeader {
        Layout.fillWidth: true
        objectName: "appearanceDecorationThemeHeader"
        title: qsTr("Window decoration theme")
        description: qsTr("Choose the installed KWin decoration used by real application windows")
    }

    Label {
        Layout.fillWidth: true
        objectName: "appearanceDecorationUnavailable"
        visible: root.settings === null
        text: qsTr("Window decoration discovery is unavailable")
        muted: true
        wrapMode: Text.Wrap
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Flow {
        Layout.fillWidth: true
        Layout.preferredHeight: childrenRect.height
        Layout.minimumHeight: childrenRect.height
        objectName: "appearanceDecorationChoices"
        visible: root.settings !== null
        spacing: Tokens.space["2"]

        Repeater {
            objectName: "appearanceDecorationRepeater"
            model: root.settings !== null ? root.settings.decorations : []
            onItemAdded: (index, card) => {
                if (index === 0 && root.firstChoice === null)
                    root.firstChoice = card
            }

            delegate: DecorationChoiceCard {
                required property var modelData

                objectName: "appearanceDecoration_" + modelData.id
                decorationName: modelData.name
                decorationKind: modelData.kind
                available: (modelData.available ?? true) && !root.editorBusy
                checked: root.settings !== null
                         && root.settings.selectedId === modelData.id
                onToggled: {
                    if (checked && root.settings !== null)
                        root.settings.selectDecoration(modelData.id)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.settings !== null
        spacing: Tokens.space["2"]

        Label {
            Layout.fillWidth: true
            objectName: "appearanceDecorationStatus"
            text: root.settings !== null ? root.settings.statusText : ""
            muted: true
            wrapMode: Text.Wrap
            Accessible.name: text
        }

        Button {
            objectName: "appearanceApplyDecoration"
            text: qsTr("Use decoration")
            emphasized: true
            available: root.settings !== null && root.settings.applyAvailable
            onClicked: root.settings.applySelection()
        }
    }

    Label {
        Layout.fillWidth: true
        objectName: "appearanceDecorationError"
        visible: root.settings !== null && root.settings.errorText.length > 0
        text: visible ? root.settings.errorText : ""
        wrapMode: Text.Wrap
        color: Tokens.fg.default
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
}
