// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var startupSettings
    signal closeRequested()

    readonly property Item firstFocusTarget:
        addNameField.enabled ? addNameField : root
    readonly property bool compact: width < 560

    title: qsTr("Startup applications")
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
            objectName: "startupPageHeading"
            Layout.fillWidth: true
            text: qsTr("Startup applications")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Choose what launches when you log in.")
            wrapMode: Text.Wrap
            muted: true
        }

        Label {
            objectName: "startupPageError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.startupSettings.errorText
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "startupFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: T.ScrollBar {}

            ColumnLayout {
                id: formColumn
                width: viewport.width
                spacing: Tokens.space["3"]

                FormSurface {
                    Layout.fillWidth: true
                    padding: Tokens.space["3"]
                    contentItem: ColumnLayout {
                        spacing: Tokens.space["2"]

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Add a command")
                            description: qsTr(
                                "Runs a command every time you log in")
                        }

                        FormRow {
                            Layout.fillWidth: true
                            label: qsTr("Name")
                            editor: TextField {
                                id: addNameField
                                objectName: "startupAddName"
                                Layout.fillWidth: true
                                accessibleName: qsTr("Name")
                            }
                        }

                        FormRow {
                            Layout.fillWidth: true
                            label: qsTr("Command")
                            editor: TextField {
                                id: addCommandField
                                objectName: "startupAddCommand"
                                Layout.fillWidth: true
                                accessibleName: qsTr("Command")
                                Keys.onReturnPressed: addButton.clicked()
                            }
                        }

                        Button {
                            id: addButton
                            objectName: "startupAddButton"
                            Layout.alignment: Qt.AlignRight
                            available: addNameField.text.trim().length > 0
                                       && addCommandField.text.trim().length > 0
                            text: qsTr("Add")
                            accessibleDescription: qsTr(
                                "Add this command to startup applications")
                            onClicked: {
                                if (!available) {
                                    return
                                }
                                if (root.startupSettings.addCommand(
                                        addNameField.text, addCommandField.text)) {
                                    addNameField.text = ""
                                    addCommandField.text = ""
                                }
                            }
                        }
                    }
                }

                SectionHeader {
                    Layout.fillWidth: true
                    title: qsTr("Startup entries")
                }

                Repeater {
                    id: entryRepeater
                    objectName: "startupEntryRepeater"
                    model: root.startupSettings.entries.length

                    delegate: FormSurface {
                        id: entryRow
                        required property int index
                        readonly property var modelData:
                            root.startupSettings.entries[index] ?? null
                        visible: modelData !== null
                        Layout.fillWidth: true
                        padding: Tokens.space["3"]
                        Accessible.name: entryRow.modelData?.name ?? ""

                        contentItem: RowLayout {
                            spacing: Tokens.space["3"]

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Tokens.space["1"]

                                Label {
                                    Layout.fillWidth: true
                                    text: entryRow.modelData?.name ?? ""
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: text.length > 0
                                    text: entryRow.modelData?.comment ?? ""
                                    muted: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    objectName: "startupIneligibility_" + (entryRow.modelData?.id ?? "")
                                    Layout.fillWidth: true
                                    visible: text.length > 0
                                    text: entryRow.modelData?.ineligibilityReason ?? ""
                                    muted: true
                                    wrapMode: Text.Wrap
                                    Accessible.name: text
                                }
                            }

                            Switch {
                                id: enabledSwitch
                                objectName: "startupEnabled_" + (entryRow.modelData?.id ?? "")
                                text: qsTr("Enabled at login")
                                checked: entryRow.modelData?.enabled ?? false
                                onToggled: entryRow.modelData !== null
                                    && root.startupSettings.setEnabled(
                                           entryRow.modelData.id, checked)
                            }

                            Button {
                                id: removeButton
                                objectName: "startupRemove_" + (entryRow.modelData?.id ?? "")
                                visible: entryRow.modelData?.custom ?? false
                                available: true
                                emphasized: false
                                text: qsTr("Remove")
                                accessibleDescription: qsTr("Remove %1 from startup applications")
                                    .arg(entryRow.modelData?.name ?? "")
                                onClicked: entryRow.modelData !== null
                                    && root.startupSettings.removeCustom(
                                           entryRow.modelData.id)
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: entryRepeater.count === 0
                    text: qsTr("No startup applications are reported right now.")
                    muted: true
                }
            }
        }
    }
}
