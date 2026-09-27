// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// ADR-0270: the File Manager's QindaTK views wear the same colours and font
// as the rest of the window. ADR-0116 keeps the Qt platform theme the File
// Manager's only appearance source -- it publishes no QST tokens, so
// QindaTK.QindaQt's QindaQtTheme has nothing to read here -- and this feeds
// that palette into Tk.Theme's base roles instead. Tk.Theme is process-wide;
// every window of the process applies the same palette.
QtObject {
    id: root

    required property var window

    function syncPalette() {
        // AGENT-GUARD: Read roles afresh on the owning window's notification.
        // Qt can replace its inherited application palette without notifying
        // bindings through a var holding that same palette object. Caching a
        // role map here left dark file-view colors inside light Qt chrome.
        const palette = root.window.palette
        Tk.Theme.applyRoles({
            "bg": palette.window,
            "surface": palette.window,
            "panel": palette.base,
            "border": palette.mid,
            "text": palette.text,
            "textMuted": palette.placeholderText,
            "accent": palette.highlight,
            "accentContrast": palette.highlightedText,
            "dark": 0.2126 * palette.window.r + 0.7152 * palette.window.g
                    + 0.0722 * palette.window.b < 0.5
        })
    }

    property Connections paletteChanges: Connections {
        target: root.window
        function onPaletteChanged() { root.syncPalette() }
    }
    Component.onCompleted: {
        root.syncPalette()
        // The application font (F1 font preferences); an empty family keeps
        // the toolkit's own.
        const font = Qt.application.font
        Tk.Theme.setFontFamilies(font ? font.family : "", "")
    }
}
