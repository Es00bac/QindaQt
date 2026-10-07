// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Tokens 1.0
ColumnLayout {
    id: root
    required property var row
    spacing: 4
    objectName: "agentUsageProviderRow"
    Label {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        text: root.row.name + " · " + root.row.state
        color: Tokens.fg.default
        font.bold: true
        wrapMode: Text.Wrap
        Accessible.role: Accessible.Heading
    }
    Label {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        text: root.row.scope + "\n" + root.row.tokenScope + "\n" + root.row.tokens
              + "\n" + root.row.costScope + "\n" + root.row.cost
        color: Tokens.fg.default
        wrapMode: Text.Wrap
    }
    Repeater {
        model: root.row.quotas
        Label {
            textFormat: Text.PlainText
            required property string modelData
            Layout.fillWidth: true
            text: modelData
            color: Tokens.fg.default
            wrapMode: Text.Wrap
        }
    }
    Label {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        text: root.row.observed + (root.row.source !== "" ? " · " + root.row.source : "")
              + (root.row.detail !== "" ? "\n" + root.row.detail : "")
        color: Tokens.fg.muted
        wrapMode: Text.Wrap
    }
}
