// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var defaultApplicationsSettings
    signal closeRequested()

    readonly property Item firstFocusTarget: rowRepeater.count > 0
        ? rowRepeater.itemAt(0).selectorItem : root
    readonly property bool compact: width < 560

    title: qsTr("Default applications")
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
            objectName: "defaultApplicationsPageHeading"
            Layout.fillWidth: true
            text: qsTr("Default applications")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            objectName: "defaultApplicationsTerminalNotice"
            Layout.fillWidth: true
            text: qsTr("Terminal has no standard default-application mechanism and is not listed here.")
            muted: true
            wrapMode: Text.Wrap
            Accessible.name: text
        }

        Label {
            objectName: "defaultApplicationsErrorText"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.defaultApplicationsSettings.errorText
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "defaultApplicationsFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Default applications scroll position")
            }

            FormSurface {
                id: formSurface
                width: parent.width
                padding: Tokens.space["3"]

                contentItem: ColumnLayout {
                    spacing: Tokens.space["2"]

                    Repeater {
                        id: rowRepeater
                        model: root.defaultApplicationsSettings.rows

                        delegate: RowLayout {
                            id: categoryRow
                            required property var modelData
                            readonly property Item selectorItem: selector
                            Layout.fillWidth: true
                            spacing: Tokens.space["2"]

                            Label {
                                Layout.preferredWidth: 160
                                text: categoryRow.modelData.label
                                Accessible.name: text
                            }
                            ComboBox {
                                id: selector
                                objectName: "defaultApplicationSelector_" + categoryRow.modelData.id
                                Layout.fillWidth: true
                                readonly property var options: {
                                    const opts = [{ "value": "", "label": qsTr("None") }]
                                    for (const option of categoryRow.modelData.options)
                                        opts.push({ "value": option.id, "label": option.name })
                                    if (categoryRow.modelData.currentId.length > 0
                                            && opts.findIndex(function(option) {
                                                return option.value === categoryRow.modelData.currentId }) < 0)
                                        opts.push({
                                            "value": categoryRow.modelData.currentId,
                                            "label": categoryRow.modelData.currentId })
                                    return opts
                                }
                                model: options
                                textRole: "label"
                                valueRole: "value"
                                currentIndex: options.findIndex(function(option) {
                                    return option.value === categoryRow.modelData.currentId
                                })
                                accessibleDescription: categoryRow.modelData.accessibleDescription
                                onActivated: index => {
                                    if (index >= 0 && index < options.length)
                                        root.defaultApplicationsSettings.setDefaultApplication(
                                            categoryRow.modelData.id, options[index].value)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
