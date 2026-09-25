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

    // The window's palette (ApplicationWindow.palette).
    required property var palette

    readonly property var roles: ({
        "bg": root.palette.window,
        "surface": root.palette.window,
        "panel": root.palette.base,
        "border": root.palette.mid,
        "text": root.palette.text,
        "textMuted": root.palette.placeholderText,
        "accent": root.palette.highlight,
        "accentContrast": root.palette.highlightedText,
        "dark": 0.2126 * root.palette.window.r + 0.7152 * root.palette.window.g
                + 0.0722 * root.palette.window.b < 0.5
    })

    onRolesChanged: Tk.Theme.applyRoles(root.roles)
    Component.onCompleted: {
        Tk.Theme.applyRoles(root.roles)
        // The application font (F1 font preferences); an empty family keeps
        // the toolkit's own.
        const font = Qt.application.font
        Tk.Theme.setFontFamilies(font ? font.family : "", "")
    }
}
