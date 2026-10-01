// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaTK as Tk
Window {
    id: root
    objectName: "portalConsentWindow"
    visible: false
    width: 500
    height: Math.min(720, content.implicitHeight + 48)
    color: Tk.Theme.color.bg
    title: consent.title
    onClosing: consent.deny()
    Flickable {
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: width
        contentHeight: content.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        focus: true
        Keys.onEscapePressed: consent.deny()
        Tk.ScrollBar.vertical: Tk.ScrollBar {}
    ColumnLayout {
        id: content
        width: parent.width
        spacing: 16
        visible: consent.ready
        Tk.Label { Layout.fillWidth: true; text: consent.title; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.bold: true }
        Tk.Label { Layout.fillWidth: true; text: consent.appId.length ? consent.appId : qsTr("Local application"); textFormat: Text.PlainText; wrapMode: Text.Wrap }
        Tk.Label { Layout.fillWidth: true; text: consent.subtitle; textFormat: Text.PlainText; wrapMode: Text.Wrap; visible: text.length > 0 }
        Tk.Label { Layout.fillWidth: true; text: consent.body; textFormat: Text.PlainText; wrapMode: Text.Wrap; visible: text.length > 0 }
        Repeater {
            model: consent.choices
            delegate: ColumnLayout {
                id: choiceRow
                required property var modelData
                Layout.fillWidth: true
                Tk.CheckBox {
                    visible: choiceRow.modelData.options.length === 0
                    text: choiceRow.modelData.label
                    checked: choiceRow.modelData.checked
                    onToggled: consent.choose(choiceRow.modelData.id, checked ? "true" : "false")
                }
                Tk.Label { text: choiceRow.modelData.label; textFormat: Text.PlainText; visible: choiceRow.modelData.options.length > 0 }
                Tk.ComboBox {
                    visible: choiceRow.modelData.options.length > 0
                    model: choiceRow.modelData.options
                    textRole: "label"
                    currentIndex: choiceRow.modelData.index
                    onActivated: consent.choose(choiceRow.modelData.id, choiceRow.modelData.options[index].id)
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Tk.Button { objectName: "portalDenyButton"; text: consent.denyLabel; onClicked: consent.deny() }
            Tk.Button { objectName: "portalGrantButton"; text: consent.grantLabel; enabled: consent.ready; onClicked: consent.grant() }
        }
    }
    }
}
