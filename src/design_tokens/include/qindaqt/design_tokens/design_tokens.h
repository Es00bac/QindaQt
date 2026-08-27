// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/accessibility_inputs.h"

#include <QColor>
#include <QString>
#include <QVariantMap>

namespace QindaQt::DesignTokens {

struct BackgroundTokens final {
    QColor base;
    QColor raised;
    QColor highest;
    [[nodiscard]] bool operator==(const BackgroundTokens &) const = default;
};

struct ForegroundTokens final {
    QColor defaultColor;
    QColor muted;
    QColor disabled;
    [[nodiscard]] bool operator==(const ForegroundTokens &) const = default;
};

struct AccentTokens final {
    QColor defaultColor;
    QColor foreground;
    QColor subtle;
    [[nodiscard]] bool operator==(const AccentTokens &) const = default;
};

struct StateTokens final {
    QColor hover;
    QColor pressed;
    [[nodiscard]] bool operator==(const StateTokens &) const = default;
};

struct StatusPair final {
    QColor background;
    QColor foreground;
    [[nodiscard]] bool operator==(const StatusPair &) const = default;
};

struct StatusTokens final {
    StatusPair success;
    StatusPair warning;
    StatusPair info;
    [[nodiscard]] bool operator==(const StatusTokens &) const = default;
};

struct DangerTokens final {
    QColor defaultColor;
    QColor foreground;
    [[nodiscard]] bool operator==(const DangerTokens &) const = default;
};

struct RadiusTokens final {
    double small = 0.0;
    double medium = 0.0;
    double large = 0.0;
    [[nodiscard]] bool operator==(const RadiusTokens &) const = default;
};

struct SpacingTokens final {
    double one = 2.0;
    double two = 4.0;
    double three = 8.0;
    double four = 12.0;
    double five = 16.0;
    double six = 24.0;
    [[nodiscard]] bool operator==(const SpacingTokens &) const = default;
};

struct TypeScaleTokens final {
    QString fontFamily;
    QString monoFontFamily;
    double caption = 0.0;
    double body = 0.0;
    double subtitle = 0.0;
    double title = 0.0;
    double display = 0.0;
    [[nodiscard]] bool operator==(const TypeScaleTokens &) const = default;
};

struct MotionTokens final {
    int instant = 0;
    int shortDuration = 0;
    int base = 0;
    int longDuration = 0;
    [[nodiscard]] bool operator==(const MotionTokens &) const = default;
};

struct ElevationLevel final {
    bool backgroundBlur = false;
    int blurRadius = 0;
    int verticalOffset = 0;
    double shadowOpacity = 0.0;
    [[nodiscard]] bool operator==(const ElevationLevel &) const = default;
};

struct ElevationTokens final {
    ElevationLevel one;
    ElevationLevel two;
    ElevationLevel three;
    [[nodiscard]] bool operator==(const ElevationTokens &) const = default;
};

// AGENT-CONTRACT: This is an immutable-by-interface, thread-neutral value.
// Replacing a complete copy is allowed; individual roles have no mutators.
// QST revision 1 may only gain compatible implementation fixes, never renamed
// or removed role keys. See architecture/design-tokens.md and ADR-0013.
class DesignTokens final {
public:
    static constexpr int qstRevision = 1;

    DesignTokens(const DesignTokens &) = default;
    DesignTokens(DesignTokens &&) noexcept = default;
    DesignTokens &operator=(const DesignTokens &) = default;
    DesignTokens &operator=(DesignTokens &&) noexcept = default;
    ~DesignTokens() = default;

    [[nodiscard]] const QString &sourceThemeId() const;
    [[nodiscard]] const AccessibilityInputs &inputs() const;
    [[nodiscard]] const BackgroundTokens &background() const;
    [[nodiscard]] const ForegroundTokens &foreground() const;
    [[nodiscard]] const AccentTokens &accent() const;
    [[nodiscard]] const StateTokens &state() const;
    [[nodiscard]] const QColor &focusRing() const;
    [[nodiscard]] const QColor &divider() const;
    [[nodiscard]] const QColor &strongOutline() const;
    [[nodiscard]] const StatusTokens &status() const;
    [[nodiscard]] const DangerTokens &danger() const;
    [[nodiscard]] const RadiusTokens &radius() const;
    [[nodiscard]] const SpacingTokens &spacing() const;
    [[nodiscard]] const TypeScaleTokens &typeScale() const;
    [[nodiscard]] const MotionTokens &motion() const;
    [[nodiscard]] const ElevationTokens &elevation() const;

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] bool operator==(const DesignTokens &) const = default;

private:
    friend class DesignTokenDeriver;

    DesignTokens(QString sourceThemeId,
                 AccessibilityInputs inputs,
                 BackgroundTokens background,
                 ForegroundTokens foreground,
                 AccentTokens accent,
                 StateTokens state,
                 QColor focusRing,
                 QColor divider,
                 QColor strongOutline,
                 StatusTokens status,
                 DangerTokens danger,
                 RadiusTokens radius,
                 SpacingTokens spacing,
                 TypeScaleTokens typeScale,
                 MotionTokens motion,
                 ElevationTokens elevation);

    QString m_sourceThemeId;
    AccessibilityInputs m_inputs;
    BackgroundTokens m_background;
    ForegroundTokens m_foreground;
    AccentTokens m_accent;
    StateTokens m_state;
    QColor m_focusRing;
    QColor m_divider;
    QColor m_strongOutline;
    StatusTokens m_status;
    DangerTokens m_danger;
    RadiusTokens m_radius;
    SpacingTokens m_spacing;
    TypeScaleTokens m_typeScale;
    MotionTokens m_motion;
    ElevationTokens m_elevation;
};

} // namespace QindaQt::DesignTokens
