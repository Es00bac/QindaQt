// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>

#include <optional>

namespace QindaQt::Apps::SettingsAccessibility {

// AGENT-CONTRACT: The route scopes its Settings1 client to exactly these
// four schema v2 keys. The schema's screen-reader key exists but has no
// consumer yet, so it is deliberately not scoped, read, or written here; the
// boundary scan rejects any route source naming it
// (see docs/wiki/apps/accessibility-settings.md).
struct AccessibilityKeys final {
    static constexpr char HighContrast[] = "accessibility.highContrast";
    static constexpr char ReducedMotion[] = "accessibility.reducedMotion";
    static constexpr char ReducedTransparency[] = "accessibility.reducedTransparency";
    static constexpr char TextScale[] = "accessibility.textScale";

    // Deterministic commit order; applyDraft() writes changed keys in this order.
    [[nodiscard]] static QStringList scopedKeys();
};

// Typed projection of the four scoped values. Bounds mirror the schema v2
// constraints for `accessibility.textScale` and the QST-1
// AccessibilityInputs normalization range; the route never writes a value
// the service would reject.
struct AccessibilityValues final {
    static constexpr double DefaultTextScale = 1.0;
    static constexpr double MinimumTextScale = 0.5;
    static constexpr double MaximumTextScale = 3.0;

    bool highContrast = false;
    bool reducedMotion = false;
    bool reducedTransparency = false;
    double textScale = DefaultTextScale;

    [[nodiscard]] static bool isValidTextScale(double scale) noexcept;
    // Fails closed: every scoped key must be present with a Boolean or an
    // in-range numeric value; integral JSON numbers are accepted for the scale.
    [[nodiscard]] static std::optional<AccessibilityValues>
    fromVariantMap(const QVariantMap &values, QString *error = nullptr);
    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] QVariant value(const QString &key) const;
    // True when `wire` (a service-reported value) denotes the same value as
    // `intended` for `key`, tolerating integral wire numbers for the scale.
    [[nodiscard]] static bool sameValue(const QString &key, const QVariant &wire,
                                        const QVariant &intended);
    [[nodiscard]] bool operator==(const AccessibilityValues &other) const noexcept;
    [[nodiscard]] bool operator!=(const AccessibilityValues &other) const noexcept
    {
        return !(*this == other);
    }
};

} // namespace QindaQt::Apps::SettingsAccessibility
