// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

// Dismissible shortcut cheat-sheet pinned to the desktop background surface
// (ADR-0084). One instance is attached per wallpaper output by
// ShortcutNoteController; only the primary output renders it. The card never
// takes keyboard focus: the hosting background window is desktop-scoped and
// keyboard-inactive, and every interactive element below uses NoFocus.
//
// AGENT-GUARD: shortcut text advertises DEFAULT bindings only. The public
// GlobalShortcutRegistrar seam cannot report the user's current mapped
// sequences, so never replace the "defaults" labeling with live key text
// without adding that capability first. Do not advertise unshipped actions.
Rectangle {
    id: root

    required property var note
    required property string screenName

    // Fallbacks are the QindaPunk Nightfall ink/slate/amber tokens; the live
    // theme map (Porcelain when light) always overrides them.
    readonly property var theme: note !== null ? note.theme ?? ({}) : ({})
    readonly property var colors: root.theme.colors ?? ({})
    readonly property string keyFontFamily: root.theme.monoFontFamily ?? ""
    readonly property string textFontFamily: root.theme.fontFamily ?? ""
    readonly property var rows: [
        { keys: qsTr("Meta+F1"), label: qsTr("Show or hide this note"), own: true },
        { keys: qsTr("Meta+Shift + drag"), label: qsTr("Combine windows"), own: false },
        { keys: qsTr("Meta+Shift+D"), label: qsTr("Enter docking mode"), own: false },
        { keys: qsTr("Arrow keys"), label: qsTr("Choose docking edge"), own: false },
        { keys: qsTr("Esc"), label: qsTr("Cancel"), own: false },
        { keys: qsTr("Enter"), label: qsTr("Confirm"), own: false }
    ]

    objectName: "shortcutNoteCard"
    visible: note !== null && note.noteVisible
             && screenName === note.primaryScreenName
    // ShortcutNoteController creates the card before assigning its visual
    // parent, and wallpaper teardown can clear that parent before QObject
    // ownership destroys the card. Keep every parent read valid across both
    // transitions so routine lifecycle changes do not emit binding errors.
    anchors.top: parent ? parent.top : undefined
    anchors.right: parent ? parent.right : undefined
    anchors.topMargin: (note !== null ? note.topInset : 0) + 16
    anchors.rightMargin: (note !== null ? note.rightInset : 0) + 16
    width: parent
           ? Math.min(320, parent.width - anchors.rightMargin - 16)
           : 0
    implicitHeight: parent
                    ? Math.min(content.implicitHeight + 24,
                               parent.height - anchors.topMargin - 16)
                    : 0
    radius: Math.min(theme.cornerRadius ?? 10, 14)
    color: colors.surface ?? "#192939"
    border.width: 1
    border.color: colors.border ?? "#526170"

    // QindaPunk amber identity stripe.
    Rectangle {
        id: accentStripe
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 3
        radius: 1.5
        color: colors.accent ?? "#D98A32"
    }

    Column {
        id: content
        anchors.fill: parent
        anchors.margins: 12
        topPadding: 10
        spacing: 6

        Text {
            id: title
            width: content.width
            text: qsTr("Desktop shortcuts")
            color: colors.text ?? "#F2EFE8"
            font.family: root.textFontFamily.length > 0 ? root.textFontFamily : ""
            font.pixelSize: 15
            font.bold: true
            elide: Text.ElideRight
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Repeater {
            model: root.rows

            Item {
                id: row
                required property var modelData
                width: content.width
                implicitHeight: Math.max(keycap.implicitHeight, label.implicitHeight) + 4

                Rectangle {
                    id: keycap
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: keyText.implicitWidth + 14
                    height: keyText.implicitHeight + 8
                    radius: 4
                    // The note's own toggle is the only default that can be
                    // self-demonstrated; mark it with the amber accent fill.
                    color: row.modelData.own
                          ? colors.accent ?? "#D98A32"
                          : colors.surfaceRaised ?? "#273746"
                    border.width: 1
                    border.color: row.modelData.own
                                  ? colors.accent ?? "#D98A32"
                                  : colors.border ?? "#526170"

                    Text {
                        id: keyText
                        anchors.centerIn: parent
                        text: row.modelData.keys
                        color: row.modelData.own
                              ? colors.accentText ?? "#1B1309"
                              : colors.text ?? "#F2EFE8"
                        font.family: root.keyFontFamily.length > 0
                                     ? root.keyFontFamily : "monospace"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Text {
                    id: label
                    anchors.left: keycap.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.label
                    color: colors.textMuted ?? "#B0B6B7"
                    font.family: root.textFontFamily.length > 0
                                 ? root.textFontFamily : ""
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Accessible.role: Accessible.StaticText
                    Accessible.name: row.modelData.label
                }
            }
        }

        Text {
            id: defaultsFootnote
            objectName: "shortcutNoteDefaultsFootnote"
            width: content.width
            topPadding: 2
            text: qsTr("Listed shortcuts are defaults.")
            color: colors.textMuted ?? "#B0B6B7"
            font.family: root.textFontFamily.length > 0
                         ? root.textFontFamily : ""
            font.pixelSize: 9
            font.italic: true
            elide: Text.ElideRight
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Button {
            id: dismissButton
            objectName: "shortcutNoteDismissButton"
            width: content.width
            height: 30
            // AGENT-GUARD: NoFocus keeps the click from moving focus into the
            // desktop background window; the window itself must never receive
            // keyboard activation (see WallpaperController's layer contract).
            focusPolicy: Qt.NoFocus
            activeFocusOnTab: false
            hoverEnabled: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Dismiss the shortcut note")

            background: Rectangle {
                radius: 6
                color: dismissButton.hovered || dismissButton.pressed
                       ? colors.accent ?? "#D98A32"
                       : colors.surfaceRaised ?? "#273746"
                border.width: 1
                border.color: colors.accent ?? "#D98A32"
            }
            contentItem: Text {
                text: qsTr("Got it")
                color: dismissButton.hovered || dismissButton.pressed
                       ? colors.accentText ?? "#1B1309"
                       : colors.text ?? "#F2EFE8"
                font.family: root.textFontFamily.length > 0
                             ? root.textFontFamily : ""
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                if (root.note !== null) {
                    root.note.dismiss();
                }
            }
        }
    }
}
