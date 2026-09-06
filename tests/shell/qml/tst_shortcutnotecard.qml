// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as ShellComponents

Item {
    id: testRoot
    width: 1280
    height: 800

    QtObject {
        id: fakeNote
        property bool noteVisible: true
        property string primaryScreenName: "primary"
        property int topInset: 30
        property int rightInset: 0
        property var theme: ({
            "fontFamily": "Inter",
            "monoFontFamily": "JetBrains Mono",
            "cornerRadius": 10,
            "colors": {
                "surface": "#192939", "surfaceRaised": "#273746",
                "border": "#526170", "text": "#F2EFE8",
                "textMuted": "#B0B6B7", "accent": "#D98A32",
                "accentText": "#1B1309"
            }
        })
        property int dismissCalls: 0
        function dismiss() { ++dismissCalls; }
    }

    Component {
        id: cardComponent
        ShellComponents.ShortcutNoteCard {
            note: fakeNote
            screenName: "primary"
        }
    }

    TestCase {
        name: "ShortcutNoteCard"
        when: windowShown

        function test_visibleOnlyOnPrimaryWhenNoteVisible() {
            const card = createTemporaryObject(cardComponent, testRoot,
                                               { "screenName": "primary" });
            verify(card !== null);
            verify(card.visible);
            compare(card.visible, fakeNote.noteVisible);

            fakeNote.noteVisible = false;
            verify(!card.visible);
            fakeNote.noteVisible = true;

            card.screenName = "secondary";
            verify(!card.visible);
            card.screenName = "primary";
            verify(card.visible);

            fakeNote.primaryScreenName = "secondary";
            verify(!card.visible);
            fakeNote.primaryScreenName = "primary";
            verify(card.visible);
        }

        function test_placementRespectsPanelInsets() {
            const card = createTemporaryObject(cardComponent, testRoot,
                                               { "screenName": "primary" });
            verify(card !== null);
            compare(card.anchors.topMargin, fakeNote.topInset + 16);
            compare(card.anchors.rightMargin, fakeNote.rightInset + 16);
            fakeNote.topInset = 72;
            compare(card.anchors.topMargin, 88);
        }

        function test_dismissButtonIsClickableWithoutFocusCapture() {
            const card = createTemporaryObject(cardComponent, testRoot,
                                               { "screenName": "primary" });
            verify(card !== null);
            const button = findChild(card, "shortcutNoteDismissButton");
            verify(button !== null);
            compare(button.focusPolicy, Qt.NoFocus);

            const before = fakeNote.dismissCalls;
            mouseClick(button);
            compare(fakeNote.dismissCalls, before + 1);

            // No focus capture: neither the card nor its button may hold
            // focus after a click; the hosting desktop background window
            // stays keyboard-inactive.
            verify(!button.activeFocus);
            verify(!card.activeFocus);
            verify(!card.focus);
        }

        function test_advertisesOnlyAccurateDefaultShortcuts() {
            const card = createTemporaryObject(cardComponent, testRoot,
                                               { "screenName": "primary" });
            verify(card !== null);
            compare(card.rows.length, 6);

            const keycaps = [];
            for (let index = 0; index < card.rows.length; ++index) {
                keycaps.push(card.rows[index].keys);
            }
            keycaps.sort();
            compare(keycaps, ["Arrow keys", "Enter", "Esc", "Meta+F1",
                              "Meta+Shift + drag", "Meta+Shift+D"]);

            const footnote = findChild(card, "shortcutNoteDefaultsFootnote");
            verify(footnote !== null);
            verify(footnote.text.toLowerCase().indexOf("default") !== -1);
        }
    }
}
