// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

// Tokenized closed-state selector for first-party forms. The native popup and
// keyboard/type-ahead behavior remain owned by Qt Quick Controls; this wrapper
// owns only the semantic surface, text, focus, and indicator presentation.
T.ComboBox {
    id: control

    property string accessibleDescription: ""
    readonly property int transitionDuration: Tokens.motion.short

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(40, implicitContentHeight + topPadding + bottomPadding)
    leftPadding: Tokens.space["3"]
    rightPadding: indicator.width + Tokens.space["3"]
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["2"]

    Accessible.role: Accessible.ComboBox
    Accessible.name: control.editable ? control.editText : control.displayText
    Accessible.description: accessibleDescription

    // ComboBox owns the delegate model consumed by the popup ListView. The
    // popup must not declare a second delegate: Qt would keep rendering the
    // unstyled root delegateModel in that case.
    delegate: T.ItemDelegate {
        required property int index
        width: ListView.view ? ListView.view.width : control.width
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textAt(index)
            color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Tokens.radius.s
            color: parent.highlighted ? Tokens.state.hover : "transparent"
        }
    }

    // AGENT-CONTRACT: Editable consumers (including the Appearance font route)
    // depend on this content item forwarding real text input into editText.
    // Keep one content item for both selector and search use. A Text item looks
    // correct while closed but cannot receive editText/key input when a font
    // picker opts into ComboBox.editable.
    contentItem: T.TextField {
        text: control.editable ? control.editText : control.displayText
        enabled: control.enabled && control.editable
        readOnly: !control.editable
        selectByMouse: control.editable
        color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
        selectionColor: Tokens.accent.default
        selectedTextColor: Tokens.accent.fg
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.body
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        verticalAlignment: Text.AlignVCenter
        background: null
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        onTextEdited: control.editText = text
        onAccepted: control.accepted()
    }

    indicator: Text {
        x: control.mirrored ? Tokens.space["3"] : control.width - width - Tokens.space["3"]
        y: (control.height - height) / 2
        text: "⌄"
        color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.body
        verticalAlignment: Text.AlignVCenter
        Accessible.ignored: true
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: Tokens.bg.highest
        border.width: control.activeFocus ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: control.activeFocus ? Tokens.focus.ring : Tokens.outline.strong

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.hovered && control.enabled ? Tokens.state.hover : "transparent"
            Accessible.ignored: true
        }
    }

    popup: T.Popup {
        y: control.height - 1
        width: control.width
        padding: Tokens.space["1"]
        implicitHeight: Math.min(contentItem.implicitHeight + padding * 2, 320)

        contentItem: ListView {
            objectName: "comboPopupList"
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            T.ScrollBar.vertical: T.ScrollBar {
                objectName: "comboPopupScrollBar"
                policy: T.ScrollBar.AsNeeded
            }
        }

        background: Rectangle {
            radius: Tokens.radius.m
            color: Tokens.bg.raised
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.outline.strong
        }
    }
}
