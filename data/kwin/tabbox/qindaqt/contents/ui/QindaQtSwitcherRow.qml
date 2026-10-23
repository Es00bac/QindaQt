// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: row

    required property int index
    required property string caption
    required property var windowId
    property var containerIdentities: ({})
    readonly property var container: containerIdentities[String(windowId).replace(/[{}]/g, "")] ?? null
    readonly property string displayCaption: container ? container.name : caption
    required property var icon
    required property bool minimized
    required property bool selected
    required property var appearancePalette
    signal triggered

    radius: Kirigami.Units.cornerRadius
    color: selected ? (appearancePalette.accent ?? Kirigami.Theme.highlightColor)
                    : (appearancePalette.surface ?? Kirigami.Theme.backgroundColor)
    border.width: selected ? 2 : 0
    border.color: appearancePalette.accent ?? Kirigami.Theme.focusColor

    Accessible.role: Accessible.ListItem
    Accessible.name: selected ? qsTr("%1, selected").arg(displayCaption) : displayCaption
    Accessible.description: container ? qsTr("Container") : (minimized ? qsTr("Minimized") : "")
    Accessible.focused: selected

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.largeSpacing
        anchors.rightMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        Rectangle {
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium + 8
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium + 8
            radius: Kirigami.Units.cornerRadius
            // Keep the exact container accent visible even when selection
            // uses that same accent as its background.
            color: row.container ? (row.appearancePalette.surface ?? Kirigami.Theme.backgroundColor)
                                 : "transparent"
            Kirigami.Icon {
                objectName: "switcherIdentityIcon"
                anchors.centerIn: parent
                width: Kirigami.Units.iconSizes.medium
                height: Kirigami.Units.iconSizes.medium
                source: row.container ? Qt.resolvedUrl("qindaqt-container-symbolic.svg") : row.icon
                isMask: row.container !== null
                color: row.container ? row.container.color : Kirigami.Theme.textColor
                Accessible.ignored: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: row.displayCaption
                color: row.selected
                    ? (row.appearancePalette.accentText ?? Kirigami.Theme.highlightedTextColor)
                    : (row.appearancePalette.text ?? Kirigami.Theme.textColor)
                font.family: Kirigami.Theme.defaultFont.family
                font.pointSize: Kirigami.Theme.defaultFont.pointSize
                font.weight: row.selected ? Font.DemiBold : Font.Normal
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
            }

            Text {
                Layout.fillWidth: true
                visible: row.minimized
                text: qsTr("Minimized")
                color: row.selected
                    ? (row.appearancePalette.accentText ?? Kirigami.Theme.highlightedTextColor)
                    : (row.appearancePalette.textMuted ?? Kirigami.Theme.disabledTextColor)
                font.family: Kirigami.Theme.smallFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }

        Text {
            visible: row.selected
            text: qsTr("Selected")
            color: row.appearancePalette.accentText
                   ?? Kirigami.Theme.highlightedTextColor
            font.family: Kirigami.Theme.smallFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
        }
    }

    TapHandler {
        onTapped: row.triggered()
    }
}
