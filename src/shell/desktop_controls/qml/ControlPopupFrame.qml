// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Shared popup chrome for the desktop controls. It is a separate popup
// window: the layer-shell panel rejects keyboard focus and cannot paint
// outside its own surface, so every outward-facing control surface here is
// hosted in its own window and seeds focus itself (audit findings A01/A02).
// Callers add rows as children; heading and feedback are owned here.
T.Popup {
    id: popup

    property string heading: ""
    property string feedback: ""
    property Item initialFocusItem: null
    default property alias rows: body.data

    // AGENT-GUARD: keep Popup.Window. An item popup would be clipped to the
    // 26-36 px panel band and could never receive keyboard focus.
    popupType: T.Popup.Window
    modal: false
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
                 | T.Popup.CloseOnPressOutsideParent
    padding: Tokens.space["3"]
    width: Math.min(480, Math.max(240, contentItem.implicitWidth + leftPadding + rightPadding))

    onOpened: Qt.callLater(function() {
        if (popup.initialFocusItem !== null)
            popup.initialFocusItem.forceActiveFocus(Qt.PopupFocusReason)
        else
            body.forceActiveFocus(Qt.PopupFocusReason)
    })

    background: Rectangle {
        radius: Tokens.radius.l
        color: Tokens.bg.raised
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.divider
    }

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        C.Label {
            objectName: "controlPopupHeading"
            Layout.fillWidth: true
            visible: popup.heading.length > 0
            text: popup.heading
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: Tokens.space["1"]
        }

        C.Label {
            objectName: "controlPopupFeedback"
            Layout.fillWidth: true
            visible: popup.feedback.length > 0
            text: popup.feedback
            color: Tokens.status.warning.foreground
            maximumLineCount: 3
            elide: Text.ElideRight
            Accessible.role: Accessible.AlertMessage
        }
    }
}
