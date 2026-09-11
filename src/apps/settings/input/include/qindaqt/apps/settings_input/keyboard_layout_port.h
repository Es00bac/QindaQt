// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QList>
#include <QString>

#include <qindaqt/apps/settings_input/store_result.h>

class QKeySequence;

namespace QindaQt::Apps::SettingsInput {

// One configurable layout row as persisted in kxkbrc [Layout].
// `layout` is a non-empty xkb layout code; `variant` may be empty (the
// layout default variant). `title`/`variantTitle` are display strings
// resolved from the evdev catalog by the route, never persisted.
struct KeyboardLayoutSelection {
    QString layout;
    QString variant;
    QString title;
    QString variantTitle;

    [[nodiscard]] bool
    isValidIdentifier() const noexcept; // xkb code shape for layout/variant
};

// Port to the configured keyboard layouts (kxkbrc [Layout]). The layout
// switch shortcut itself is a kglobalaccel shortcut of the desktop's
// "KDE Keyboard Layout Switcher" component and is edited through the
// shortcut port, not here (ADR-0134).
class KeyboardLayoutPort {
public:
    virtual ~KeyboardLayoutPort();

    // Authority-file truth. A missing file is valid default truth (one
    // default layout chosen by the desktop); malformed rows are dropped
    // whole and reported through `error` (total decode, ADR-0134).
    [[nodiscard]] virtual QList<KeyboardLayoutSelection>
    configuredLayouts(QString *error) const = 0;

    // The list must be non-empty, free of duplicates, and use valid xkb
    // identifiers; anything else fails closed with no file change.
    [[nodiscard]] virtual StoreResult
    writeConfiguredLayouts(const QList<KeyboardLayoutSelection> &layouts,
                           QString *error) const = 0;
};

// Production adapter: KConfig file at `kxkbrcPath` plus the ConfigChanged
// announcement over `bus` that makes a running KWin re-read kxkbrc
// (ADR-0134). `Stored` requires both the announcement and KWin on the bus.
class QtKeyboardLayoutPort final : public KeyboardLayoutPort {
public:
    QtKeyboardLayoutPort(QString kxkbrcPath, QDBusConnection bus);

    [[nodiscard]] QList<KeyboardLayoutSelection>
    configuredLayouts(QString *error) const override;
    [[nodiscard]] StoreResult
    writeConfiguredLayouts(const QList<KeyboardLayoutSelection> &layouts,
                           QString *error) const override;

private:
    QString m_kxkbrcPath;
    QDBusConnection m_bus;
};

} // namespace QindaQt::Apps::SettingsInput
