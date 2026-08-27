// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/accessibility_inputs.h"

#include <QColor>
#include <QString>

#include <memory>

namespace QindaQt::Themes {
class ThemeSpec;
}

namespace QindaQt::DesignTokens {

class DesignTokens;

enum class DerivationError {
    None,
    InvalidSchemaVersion,
    MissingIdentity,
    InvalidColor,
    InvalidMetric,
};

struct DerivationResult final {
    std::shared_ptr<const DesignTokens> tokens;
    DerivationError error = DerivationError::None;
    QString diagnostic;

    [[nodiscard]] bool ok() const { return tokens != nullptr; }
};

class DesignTokenDeriver final {
public:
    // AGENT-CONTRACT: Theme loading/selection remains owned by src/themes.
    // This pure operation accepts only its public value plus caller-owned text
    // and accessibility inputs, and may run on any thread. A loader-valid
    // schema-v1 ThemeSpec always returns one complete value; malformed direct
    // construction returns a typed error and no partial tokens.
    [[nodiscard]] static DerivationResult derive(
        const QindaQt::Themes::ThemeSpec &theme,
        const AccessibilityInputs &inputs = {});

    [[nodiscard]] static double relativeLuminance(const QColor &color);
    [[nodiscard]] static double contrastRatio(const QColor &foreground,
                                              const QColor &background);
};

} // namespace QindaQt::DesignTokens
