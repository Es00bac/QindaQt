// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "osk_layout_document.h"

#include <QString>
#include <QStringList>

namespace QindaQt::Apps::Osk {

// The layout documents compiled into the keyboard, keyed by XKB layout name.
// A layout the keyboard has no document for falls back to `us`, so a finger
// always finds a keyboard even when the desktop uses a rarer layout.
class OskLayoutCatalog final {
public:
    explicit OskLayoutCatalog(QString directory = defaultDirectory());

    [[nodiscard]] static QString defaultDirectory();
    [[nodiscard]] static QString normalize(const QString &xkbName);
    [[nodiscard]] QStringList available() const;
    [[nodiscard]] bool has(const QString &xkbName) const;
    [[nodiscard]] OskLayoutDocument documentFor(const QString &xkbName, QString *error = nullptr) const;

private:
    QString m_directory;
};

} // namespace QindaQt::Apps::Osk
