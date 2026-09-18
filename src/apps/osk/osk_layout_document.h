// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace QindaQt::Apps::Osk {

// One key of the on-screen keyboard (ADR-0204). Text keys commit their text;
// the other kinds are the keyboard's own controls or forwarded keysyms.
enum class KeyKind { Text, Shift, Backspace, Symbols, Letters, Space, Enter, Hide, Layout };

struct KeyDefinition {
    KeyKind kind = KeyKind::Text;
    QString text;
    QString shiftedText;
    qreal width = 1.0;
};

using KeyRow = QList<KeyDefinition>;

// A keyboard layout document: the letters page and the symbols page of one
// XKB layout name. Documents ship as resources under `layouts/`.
struct OskLayoutDocument {
    QString name;
    QString label;
    QList<KeyRow> letters;
    QList<KeyRow> symbols;

    [[nodiscard]] bool isValid() const { return !name.isEmpty() && !letters.isEmpty(); }

    [[nodiscard]] static OskLayoutDocument fromJson(const QByteArray &json, QString *error = nullptr);
    [[nodiscard]] static OskLayoutDocument load(const QString &path, QString *error = nullptr);
};

[[nodiscard]] QString keyKindName(KeyKind kind);

} // namespace QindaQt::Apps::Osk
