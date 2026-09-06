// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/accessibility_inputs.h"

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Shell {

// AGENT-CONTRACT: One confirmed Settings1 preference set for the shell. The
// keys below are the complete shell preference scope; every key is declared in
// data/settings/schema-v2.json, so a snapshot that lacks one or carries a
// wrong type fails the total decode instead of partially trusting authority
// (mirrors the Settings appearance route's AppearanceValues contract).
struct ShellPreferenceValues final {
    QString layoutProfileId;
    QString themeId;
    QString fontFamily;
    QString wallpaper;
    QString wallpaperMode;
    DesignTokens::AccessibilityInputs accessibility;

    [[nodiscard]] bool operator==(const ShellPreferenceValues &) const = default;

    // Deterministic Settings1 scope for both the startup read and the live
    // shell settings client.
    [[nodiscard]] static QStringList scopedKeys();

    // Total decode of one scoped Settings1 snapshot. values() are already
    // normalized (fonts.pointSize clamps into the schema 6..36 range, inside
    // the wider AccessibilityInputs bounds).
    [[nodiscard]] static std::optional<ShellPreferenceValues>
    fromVariantMap(const QVariantMap &values, QString *error = nullptr);
};

// Resolves a bundled identity beneath ordered generic-data roots or an explicit readable absolute path.
[[nodiscard]] QString resolveWallpaperSource(const QString &preference,
                                             const QStringList &dataRoots);

// Startup selection precedence, in order: explicit CLI value, confirmed
// Settings1 preference, built-in fallback. Pure so the rule is testable
// without a running shell.
[[nodiscard]] QString resolveStartupProfileId(
    const QString &explicitProfileId,
    const std::optional<ShellPreferenceValues> &preferences);
[[nodiscard]] QString resolveStartupThemeId(
    const QString &explicitThemeId,
    const std::optional<ShellPreferenceValues> &preferences,
    const QString &profileDefaultThemeId);

} // namespace QindaQt::Shell
