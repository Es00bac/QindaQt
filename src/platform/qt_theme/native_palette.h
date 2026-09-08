// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QFont>
#include <QPalette>
#include <QString>
#include <optional>
#include <qindaqt/design_tokens/accessibility_inputs.h>
#include <qindaqt/themes/theme_spec.h>

namespace QindaQt::QtTheme {
// Immutable projection owned by its caller, safe to derive off the GUI thread.
// Invalid source values fail without replacing the current platform palette.
struct NativeAppearance final {
    QPalette palette;
    QFont font;
    QFont fixedFont;
    QString iconTheme;
    Qt::ColorScheme scheme = Qt::ColorScheme::Unknown;
    bool highContrast = false;
};
[[nodiscard]] std::optional<NativeAppearance> nativeAppearance(
    const Themes::ThemeSpec &theme, DesignTokens::AccessibilityInputs inputs);
}
