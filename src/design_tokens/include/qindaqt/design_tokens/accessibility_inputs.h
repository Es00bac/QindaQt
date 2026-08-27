// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

namespace QindaQt::DesignTokens {

// AGENT-CONTRACT: The app composition layer owns these validated preferences.
// QST-1 normalizes numeric bounds deterministically but never discovers,
// persists, or subscribes to settings on the caller's behalf.
struct AccessibilityInputs final {
    static constexpr double defaultBasePointSize = 10.0;
    static constexpr double minimumBasePointSize = 6.0;
    static constexpr double maximumBasePointSize = 72.0;
    static constexpr double defaultTextScale = 1.0;
    static constexpr double minimumTextScale = 0.5;
    static constexpr double maximumTextScale = 3.0;

    double basePointSize = defaultBasePointSize;
    double textScale = defaultTextScale;
    bool reducedMotion = false;
    bool reducedTransparency = false;
    bool highContrast = false;

    [[nodiscard]] AccessibilityInputs normalized() const;
    [[nodiscard]] bool operator==(const AccessibilityInputs &) const = default;
};

} // namespace QindaQt::DesignTokens
