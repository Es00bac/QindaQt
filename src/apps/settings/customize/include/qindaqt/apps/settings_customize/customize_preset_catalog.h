// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::Apps::SettingsCustomize {

// One layout preset as Settings shows it: the profile the desktop would use
// for this id (a user-store copy wins over an installed profile, the same
// precedence the shell loads, docs/wiki/shell/layout-profiles.md) plus where
// it came from.
struct LayoutPreset final {
    Profiles::LayoutProfile profile;
    // The installed profile this id names, kept even while a user copy
    // shadows it so "Restore original" can show what comes back.
    std::optional<Profiles::LayoutProfile> original;
    bool builtIn = false;   // an installed (stock) profile has this id
    bool userCopy = false;  // the writable user store holds this id

    // A built-in the user has edited: a user copy shadows the installed one.
    [[nodiscard]] bool modified() const noexcept { return builtIn && userCopy; }
    // The user's own preset: only the user store has this id.
    [[nodiscard]] bool own() const noexcept { return !builtIn && userCopy; }
};

struct PresetCatalog final {
    // Built-ins in installed catalog order, then the user's own presets in
    // load order. Empty with `error` set when anything failed to load.
    QVector<LayoutPreset> presets;
    QString error;
};

// Loads the installed profile directories (low-to-high precedence; they must
// not include the user store) and then the user store. Loading is
// all-or-nothing, like the route always was: one malformed file disables the
// page with its diagnostic instead of listing a partial, misleading set. A
// user store that does not exist yet simply contributes nothing.
[[nodiscard]] PresetCatalog loadPresetCatalog(const QStringList &stockDirectories,
                                              const QString &userDirectory);

} // namespace QindaQt::Apps::SettingsCustomize
