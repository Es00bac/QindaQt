// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace QindaQt::Link::Macros {

// X11 keysyms (modifiers first, then the key) for a Qt portable-text chord such
// as "Ctrl+Shift+T", "Alt+Tab" or "Meta". Empty when the text is not exactly
// one resolvable chord, so a macro never emits half of a combination.
// AGENT-NOTE: the compositor-side output resolves the same text through xkb;
// this table is the portal-side equivalent and must cover every name
// shortcutFor() in controller_input_device.cpp can produce.
[[nodiscard]] QList<qint32> keysymsForChord(const QString &portableText);

// Modifier names used by Binding::modifiers ("Ctrl", "Alt", "Shift", "Meta").
[[nodiscard]] qint32 modifierKeysym(const QString &name);

// Default chord for a named controller action ("accept" -> "Return"), or empty.
[[nodiscard]] QString defaultChordForAction(const QString &action);

} // namespace QindaQt::Link::Macros
