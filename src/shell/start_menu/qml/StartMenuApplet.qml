// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Panel button of the start-menu applet (ADR-0124): the worn-Luna start
// button hosting the two-column start panel. This module is pure
// presentation over the facades the panel dispatcher already owns; it keeps
// no state and starts nothing itself.
//
// AGENT-CONTRACT: both facades are borrowed and may be null (preview,
// degraded composition, grant denied — the same fail-closed rule as
// LauncherAppletController and DesktopControlsAccess). Without them the
// button renders the identical visuals at half opacity and refuses to open;
// nothing may dereference an access before `available` is true.
//
// AGENT-NOTE: every #rrggbb value in this module is a Luna dressing constant
// of the Bliss start-menu experience (ADR-0124), the same instance-level
// exception as the `luna` path in TaskListEntryButton. They are presentation
// constants of this dressing, never theme authority: content controls that
// carry their own surfaces (the search field) keep token styling.
Item {
    id: root

    property var applet: null
    property var theme: null
    property bool vertical: false
    required property var launcherAppletAccess
    required property var desktopControlsAccess

    // Which Windows start menu this instance reproduces (ADR-0224). "luna" is
    // the worn XP panel and the default, so Bliss and every existing user
    // profile keep exactly the dressing they have; "modern" is the Windows 11
    // centred card. An unknown value falls back to luna rather than rendering
    // nothing.
    readonly property string variant:
        String((applet !== null ? applet.settings ?? ({}) : ({})).variant
               ?? "luna") === "modern" ? "modern" : "luna"
    readonly property bool modern: variant === "modern"

    // AGENT-GUARD: Tokens.ready gates every access read, exactly like
    // LauncherApplet. Dropping it dereferences facades before the engine has
    // published QST-1 and crashes preview composition.
    readonly property bool available: launcherAppletAccess !== null
        && desktopControlsAccess !== null && Tokens.ready

    objectName: "startMenuApplet"
    // AGENT-GUARD: the width follows the rendered icon, label, and Luna
    // padding. A fixed width let the label spill past its chip, where the
    // next applet painted over it (the clipped "star" label). The 56 px floor
    // matches the manifest's preferred extent; vertical panels show the icon
    // alone inside it.
    implicitWidth: root.modern
        ? implicitHeight
        : Math.max(56, Math.ceil(button.implicitContentWidth
                                 + button.leftPadding
                                 + button.rightPadding))
    implicitHeight: root.modern ? 40 : 36
    opacity: available ? 1.0 : 0.5

    function openPanel() {
        if (!root.available || panelLoader.item === null)
            return
        panelLoader.item.open()
    }

    // Session truth is resolved once here and shared with the popup; both
    // fail closed to null when the facade (or its sub-object) is absent.
    readonly property var session: desktopControlsAccess !== null
        && desktopControlsAccess.systemMenu !== null
        && desktopControlsAccess.systemMenu.sessionActionsAvailable
        ? desktopControlsAccess.systemMenu.sessionActions : null

    // AGENT-GUARD: exactly one panel is built. `variant` is a stable binding,
    // so this Loader resolves once per instance and never thrashes — and a
    // Luna taskbar must not pay for the modern panel's search field, pinned
    // grid and program list, which a Popup creates with its content item
    // whether or not it ever opens. Both panels set anchorItem explicitly, so
    // the Loader's own (zero) geometry never reaches placement.
    Loader {
        id: panelLoader

        objectName: "startMenuPanelLoader"
        // AGENT-GUARD: always active. The panel must exist even when a facade
        // is missing, because the fail-closed rule is "the button renders and
        // refuses to open" (openPanel checks `available`), not "the panel is
        // absent" — a missing panel is indistinguishable from a broken one.
        sourceComponent: root.modern ? modernPanel : lunaPanel
    }

    Component {
        id: lunaPanel

        StartMenuPopup {
            // Placement anchor and axis: PanelPopup opens the panel above the
            // button on a bottom taskbar and beside it on a vertical panel.
            anchorItem: root
            vertical: root.vertical
            launcher: root.launcherAppletAccess
            controls: root.desktopControlsAccess
            onLogOffRequested: {
                close()
                logOffConfirmation.open()
            }
            onClosed: button.forceActiveFocus(Qt.PopupFocusReason)
        }
    }

    Component {
        id: modernPanel

        StartMenuModernPopup {
            anchorItem: root
            vertical: root.vertical
            launcher: root.launcherAppletAccess
            controls: root.desktopControlsAccess
            onLogOffRequested: {
                close()
                logOffConfirmation.open()
            }
            onClosed: button.forceActiveFocus(Qt.PopupFocusReason)
        }
    }

    // Same confirmation rule as the system menu's session actions (ADR-0070):
    // logout never dispatches without this dialog, and the dialog is a
    // sibling of the popup — never nested inside it.
    T.Dialog {
        id: logOffConfirmation

        objectName: "startMenuLogOffConfirmation"
        popupType: T.Popup.Window
        width: 320
        modal: true
        focus: true
        title: qsTr("Log off?")
        standardButtons: T.Dialog.Cancel | T.Dialog.Ok
        onAccepted: if (root.session !== null)
            root.session.requestLogout()
        contentItem: C.Label {
            text: qsTr("Open applications will be asked to close"
                       + " before this session ends.")
            Accessible.role: Accessible.Dialog
            Accessible.name: logOffConfirmation.title
            Accessible.description: text
        }
    }

    T.ToolButton {
        id: button

        objectName: "startMenuButton"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        hoverEnabled: true
        Accessible.role: Accessible.Button
        Accessible.name: root.available ? qsTr("Start")
                                        : qsTr("Start menu is unavailable")
        Accessible.description: root.available
                                ? qsTr("Opens the start panel") : ""
        onClicked: root.openPanel()
        Keys.onReturnPressed: root.openPanel()
        Keys.onEnterPressed: root.openPanel()
        Accessible.onPressAction: root.openPanel()

        // Luna start button padding: a short lead before the icon and a
        // longer tail after the label; vertical panels center the icon. The
        // modern button is a square tile, so it is padded evenly.
        leftPadding: root.modern ? 4 : (root.vertical ? 4 : 8)
        rightPadding: root.modern ? 4 : (root.vertical ? 4 : 14)
        topPadding: 0
        bottomPadding: 0

        contentItem: Item {
            implicitWidth: buttonContent.implicitWidth
            implicitHeight: buttonContent.implicitHeight

            Row {
                id: buttonContent

                objectName: "startMenuButtonContent"
                anchors.verticalCenter: parent.verticalCenter
                x: Math.round((parent.width - width) / 2)
                spacing: 6

                ShellIcons.Icon {
                    objectName: "startMenuButtonIcon"
                    anchors.verticalCenter: parent.verticalCenter
                    name: "start-here-kde"
                    size: root.modern ? 24 : 20
                    // The Luna glyph is white on the green pill; the modern
                    // tile has no coloured chrome, so it follows the theme.
                    color: root.modern ? Tokens.fg.default : "#ffffff"
                    fallbackText: qsTr("Start")
                    Accessible.ignored: true
                }

                Text {
                    objectName: "startMenuButtonLabel"
                    anchors.verticalCenter: parent.verticalCenter
                    // The modern start button is glyph-only, like the taskbar
                    // it belongs to; the accessible name still says "Start".
                    visible: !root.vertical && !root.modern
                    text: qsTr("start")
                    color: "#ffffff"
                    // The Luna start label: bold italic white over a
                    // one-pixel dark drop shadow.
                    style: Text.Raised
                    styleColor: Qt.rgba(0, 0, 0, 0.5)
                    font.family: "Trebuchet MS"
                    font.pointSize: 11
                    font.bold: true
                    font.italic: true
                    Accessible.ignored: true
                }
            }
        }
        // AGENT-GUARD: the two dressings are exclusive. The Luna chrome keeps
        // its gradient and scanline exactly; the modern chrome is tokenized,
        // because Windows 11's start button has no fixed period palette — it
        // follows the system accent and light/dark mode.
        background: Rectangle {
            objectName: "startMenuButtonChrome"
            radius: root.modern ? Tokens.radius.m : 4
            color: root.modern
                ? (button.down ? Tokens.bg.highest
                               : button.hovered ? Tokens.bg.raised : "transparent")
                : "transparent"
            gradient: root.modern ? null : lunaGradient

            readonly property Gradient lunaGradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: button.down ? "#2f7d2c"
                         : button.hovered ? "#4dab49" : "#3d9a3a"
                }
                GradientStop {
                    position: 1.0
                    color: button.down ? "#215e1f"
                         : button.hovered ? "#3a8c37" : "#2b7a29"
                }
            }

            // Worn-Luna bottom edge: one dark scanline separating the button
            // from the panel surface beneath it.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: !root.modern
                height: 1
                color: "#1d5c1c"
                Accessible.ignored: true
            }

            C.FocusRing {
                anchors.fill: parent
                control: button
            }
        }
    }
}
