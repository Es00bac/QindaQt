// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0

T.Control {
    id: control

    default property alias editorData: editorHost.data
    required property Item editor
    property string label: ""
    property string description: ""
    property string errorMessage: ""
    property bool required: false
    property int labelWidth: 220
    readonly property bool wide: width >= 480
    readonly property string editorAccessibleName: required
                                                   ? qsTr("%1, required").arg(label)
                                                   : label
    readonly property string editorAccessibleDescription: errorMessage.length > 0
        ? qsTr("Error. %1").arg(errorMessage) : description

    leftPadding: 0
    rightPadding: 0
    topPadding: Tokens.space["3"]
    bottomPadding: Tokens.space["3"]
    Accessible.role: Accessible.Grouping
    Accessible.name: required ? qsTr("%1, required").arg(label) : label
    Accessible.description: errorMessage.length > 0 ? errorMessage : description

    // AGENT-CONTRACT: FormRow owns the label/error relationship, while the
    // editor retains its native role and value interface. Requiring the editor
    // prevents a visually labelled but anonymously exposed form control.
    Binding {
        when: control.editor !== null
        target: control.editor ? control.editor.Accessible : null
        property: "name"
        value: control.editorAccessibleName
        restoreMode: Binding.RestoreBinding
    }

    Binding {
        when: control.editor !== null
        target: control.editor ? control.editor.Accessible : null
        property: "description"
        value: control.editorAccessibleDescription
        restoreMode: Binding.RestoreBinding
    }

    contentItem: GridLayout {
        columns: control.wide ? 2 : 1
        columnSpacing: Tokens.space["5"]
        rowSpacing: Tokens.space["2"]

        ColumnLayout {
            Layout.row: 0
            Layout.column: 0
            Layout.fillWidth: !control.wide
            Layout.preferredWidth: control.wide ? control.labelWidth : -1
            Layout.alignment: Qt.AlignTop
            spacing: Tokens.space["1"]

            Text {
                Layout.fillWidth: true
                text: control.required ? qsTr("%1 (required)").arg(control.label)
                                       : control.label
                color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                horizontalAlignment: control.mirrored ? Text.AlignRight : Text.AlignLeft
                Accessible.ignored: true
            }

            Text {
                Layout.fillWidth: true
                visible: control.description.length > 0
                text: control.description
                color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                wrapMode: Text.Wrap
                horizontalAlignment: control.mirrored ? Text.AlignRight : Text.AlignLeft
                Accessible.ignored: true
            }
        }

        Item {
            id: editorHost
            objectName: "formRowEditorHost"

            Layout.row: control.wide ? 0 : 1
            Layout.column: control.wide ? 1 : 0
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }

        Text {
            Layout.row: control.wide ? 1 : 2
            Layout.column: control.wide ? 1 : 0
            Layout.fillWidth: true
            visible: control.errorMessage.length > 0
            text: control.errorMessage
            color: Tokens.danger.default
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            wrapMode: Text.Wrap
            horizontalAlignment: control.mirrored ? Text.AlignRight : Text.AlignLeft
            Accessible.ignored: true
        }
    }
}
