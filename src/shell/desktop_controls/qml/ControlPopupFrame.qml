// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Tokenized popup chrome for the desktop controls: heading, caller-supplied
// rows, and a feedback line. Callers add rows as children; heading and
// feedback are owned here.
//
// AGENT-CONTRACT: placement is NOT owned here. QindaQt.Controls.PanelPopup is
// the one owner of panel-popup placement and of the Wayland positioner-cell
// workaround for every panel surface (start menu, launcher, per-service
// applets, these controls). This frame is its tokenized dressing; the vectors
// and the reasons live in docs/wiki/shell/panel-popup-placement.md.
C.PanelPopup {
    id: popup

    property string heading: ""
    property string feedback: ""
    property Item initialFocusItem: null
    default property alias rows: body.data

    // The desktop controls dismiss on a press anywhere outside the owning
    // control, which PanelPopup's positioner mask measures against the
    // control rather than against the 1x1 cell.
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
            color: Tokens.fg.default
            maximumLineCount: 3
            elide: Text.ElideRight
            Accessible.role: Accessible.AlertMessage
        }
    }
}
