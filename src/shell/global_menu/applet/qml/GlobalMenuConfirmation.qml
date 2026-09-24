// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic

// A shell-owned menu provider's confirmation (ADR-0260): the desktop menu's
// Log Out…, Restart…, and Shut Down… ask exactly as the system menu applet
// does, in a modal window dialog with Cancel and OK, before anything runs.
//
// AGENT-GUARD: the only answer path is access.resolveConfirmation(token, ...)
// for the facade's CURRENT token, and each token is answered once. Closing
// the dialog any other way (Escape, a press outside) declines. Never call a
// session action from QML: the provider re-checks the owner's admission when
// the answer arrives. Every renderer (one per panel instance) hosts one of
// these; only the renderer that claims the token first opens it, so one
// question never becomes one dialog per output.
Basic.Dialog {
    id: dialog

    required property var access
    required property var theme
    readonly property var colors: theme.colors ?? ({})
    readonly property var request: access !== null ? (access.confirmation ?? ({})) : ({})
    readonly property string token: String(request.token ?? "")
    property string answeredToken: ""

    function present() {
        if (token.length > 0 && access.claimConfirmation(token)) {
            answeredToken = ""
            open()
        }
    }

    function answer(accepted) {
        if (token.length === 0 || answeredToken === token)
            return
        answeredToken = token
        access.resolveConfirmation(token, accepted)
    }

    objectName: "globalMenuConfirmation"
    popupType: Popup.Window
    modal: true
    focus: true
    width: 320
    title: String(request.title ?? "")
    standardButtons: Basic.Dialog.Cancel | Basic.Dialog.Ok
    palette.window: colors.surface ?? "#222624"
    palette.windowText: colors.text ?? "#f2f1eb"
    palette.text: colors.text ?? "#f2f1eb"
    palette.base: colors.surfaceRaised ?? "#2c312e"
    palette.button: colors.surfaceRaised ?? "#2c312e"
    palette.buttonText: colors.text ?? "#f2f1eb"
    palette.highlight: colors.accent ?? "#8fc8b7"
    palette.highlightedText: colors.accentText ?? "#10201b"

    onTokenChanged: {
        if (token.length > 0)
            present()
        else if (opened)
            close()
    }
    Component.onCompleted: present()
    onAccepted: answer(true)
    onRejected: answer(false)
    onClosed: answer(false)

    contentItem: Text {
        objectName: "globalMenuConfirmationText"
        text: String(dialog.request.text ?? "")
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        color: dialog.colors.text ?? "#f2f1eb"
        Accessible.role: Accessible.Dialog
        Accessible.name: dialog.title
        Accessible.description: text
    }

    background: Rectangle {
        color: dialog.colors.surface ?? "#222624"
        border.color: dialog.colors.border ?? "#3c433f"
        radius: dialog.theme.cornerRadius ?? 6
    }
}
