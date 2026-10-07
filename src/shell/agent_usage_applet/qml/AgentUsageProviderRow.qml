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
        visible: !root.row.metricsAvailable
        text: qsTr("Usage not reported") + (root.row.detail !== ""
              && root.row.detail !== "No usage report" ? "\n" + root.row.detail : "")
        color: Tokens.fg.muted
        wrapMode: Text.Wrap
    }
    Repeater {
        model: root.row.metricsAvailable ? root.row.quotas : []
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
        visible: root.row.metricsAvailable
        text: root.row.tokenScope + " · " + root.row.tokens
        color: Tokens.fg.default
        wrapMode: Text.Wrap
    }
    Label {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        visible: root.row.metricsAvailable
        text: root.row.costScope + " · " + root.row.cost
        color: Tokens.fg.default
        wrapMode: Text.Wrap
    }
    Label {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        visible: root.row.hasObservation
        text: root.row.observed + (root.row.source !== "" ? " · " + root.row.source : "")
              + (root.row.scope !== "Scope not reported" ? " · " + root.row.scope : "")
              + (root.row.metricsAvailable && root.row.detail !== "" ? "\n" + root.row.detail : "")
        color: Tokens.fg.muted
        wrapMode: Text.Wrap
    }
}
