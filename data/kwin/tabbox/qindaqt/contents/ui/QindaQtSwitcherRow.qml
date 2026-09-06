// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: row

    required property int index
    required property string caption
    required property var icon
    required property bool minimized
    required property bool selected
    signal triggered

    radius: Kirigami.Units.cornerRadius
    color: selected ? Kirigami.Theme.highlightColor
                    : Kirigami.Theme.backgroundColor
    border.width: selected ? 2 : 0
    border.color: Kirigami.Theme.focusColor

    Accessible.role: Accessible.ListItem
    Accessible.name: selected ? qsTr("%1, selected").arg(caption) : caption
    Accessible.description: minimized ? qsTr("Minimized window or group")
                                      : qsTr("Window or group")
    Accessible.focused: selected

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.largeSpacing
        anchors.rightMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            source: row.icon
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            Accessible.ignored: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: row.caption
                color: row.selected ? Kirigami.Theme.highlightedTextColor
                                    : Kirigami.Theme.textColor
                font.family: Kirigami.Theme.defaultFont.family
                font.pointSize: Kirigami.Theme.defaultFont.pointSize
                font.weight: row.selected ? Font.DemiBold : Font.Normal
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
            }

            Text {
                Layout.fillWidth: true
                text: row.minimized ? qsTr("Minimized window or group")
                                    : qsTr("Window or group")
                color: row.selected ? Kirigami.Theme.highlightedTextColor
                                    : Kirigami.Theme.disabledTextColor
                font.family: Kirigami.Theme.smallFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }

        Text {
            visible: row.selected
            text: qsTr("Selected")
            color: Kirigami.Theme.highlightedTextColor
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
