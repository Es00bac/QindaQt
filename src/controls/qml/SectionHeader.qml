// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0

T.Control {
    id: control

    property string title: ""
    property string description: ""

    leftPadding: 0
    rightPadding: 0
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["2"]
    Accessible.role: Accessible.StaticText
    Accessible.name: title
    Accessible.description: description

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        Text {
            objectName: "sectionHeaderTitle"
            Layout.fillWidth: true
            text: control.title
            color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            // Page titles belong to the route. This component labels a
            // section within that page, so it deliberately stays one rung
            // lower in the shared type hierarchy.
            font.pointSize: Tokens.type.subtitle
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            Accessible.ignored: true
        }

        Text {
            objectName: "sectionHeaderDescription"
            Layout.fillWidth: true
            visible: control.description.length > 0
            text: control.description
            color: control.enabled ? Tokens.fg.muted : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            wrapMode: Text.Wrap
            Accessible.ignored: true
        }
    }
}
