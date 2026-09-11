// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_accessibility/accessibility_values.h>

#include <QtCore/QMetaType>
#include <QtCore/QtGlobal>

namespace QindaQt::Apps::SettingsAccessibility {
namespace {

[[nodiscard]] bool isNumericType(int metaTypeId) noexcept
{
    switch (metaTypeId) {
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::optional<bool> decodeBool(const QVariantMap &values,
                                             const char *key, QString *error)
{
    const QVariant value = values.value(QLatin1String(key));
    if (value.metaType().id() != QMetaType::Bool) {
        if (error != nullptr) {
            *error = QStringLiteral("%1 must be a Boolean").arg(QLatin1String(key));
        }
        return std::nullopt;
    }
    return value.toBool();
}

[[nodiscard]] std::optional<double> decodeScale(const QVariant &value, QString *error)
{
    if (!isNumericType(value.metaType().id())) {
        if (error != nullptr) {
            *error = QStringLiteral("%1 must be a number")
                         .arg(QLatin1String(AccessibilityKeys::TextScale));
        }
        return std::nullopt;
    }
    const double scale = value.toDouble();
    if (!AccessibilityValues::isValidTextScale(scale)) {
        if (error != nullptr) {
            *error = QStringLiteral("%1 must be between %2 and %3")
                         .arg(QLatin1String(AccessibilityKeys::TextScale))
                         .arg(AccessibilityValues::MinimumTextScale)
                         .arg(AccessibilityValues::MaximumTextScale);
        }
        return std::nullopt;
    }
    return scale;
}

} // namespace

QStringList AccessibilityKeys::scopedKeys()
{
    return {QLatin1String(HighContrast), QLatin1String(ReducedMotion),
            QLatin1String(ReducedTransparency), QLatin1String(TextScale)};
}

bool AccessibilityValues::isValidTextScale(double scale) noexcept
{
    return qIsFinite(scale) && scale >= MinimumTextScale && scale <= MaximumTextScale;
}

std::optional<AccessibilityValues>
AccessibilityValues::fromVariantMap(const QVariantMap &values, QString *error)
{
    const auto highContrast = decodeBool(values, AccessibilityKeys::HighContrast, error);
    if (!highContrast) {
        return std::nullopt;
    }
    const auto reducedMotion = decodeBool(values, AccessibilityKeys::ReducedMotion, error);
    if (!reducedMotion) {
        return std::nullopt;
    }
    const auto reducedTransparency =
        decodeBool(values, AccessibilityKeys::ReducedTransparency, error);
    if (!reducedTransparency) {
        return std::nullopt;
    }
    const auto textScale = decodeScale(
        values.value(QLatin1String(AccessibilityKeys::TextScale)), error);
    if (!textScale) {
        return std::nullopt;
    }
    return AccessibilityValues{.highContrast = *highContrast,
                               .reducedMotion = *reducedMotion,
                               .reducedTransparency = *reducedTransparency,
                               .textScale = *textScale};
}

QVariantMap AccessibilityValues::toVariantMap() const
{
    return {{QLatin1String(AccessibilityKeys::HighContrast), highContrast},
            {QLatin1String(AccessibilityKeys::ReducedMotion), reducedMotion},
            {QLatin1String(AccessibilityKeys::ReducedTransparency), reducedTransparency},
            {QLatin1String(AccessibilityKeys::TextScale), textScale}};
}

QVariant AccessibilityValues::value(const QString &key) const
{
    return toVariantMap().value(key);
}

bool AccessibilityValues::sameValue(const QString &key, const QVariant &wire,
                                    const QVariant &intended)
{
    if (key == QLatin1String(AccessibilityKeys::TextScale)) {
        QString ignored;
        const auto decoded = decodeScale(wire, &ignored);
        return decoded.has_value() && qFuzzyCompare(*decoded, intended.toDouble());
    }
    return wire.metaType().id() == QMetaType::Bool
        && intended.metaType().id() == QMetaType::Bool
        && wire.toBool() == intended.toBool();
}

bool AccessibilityValues::operator==(const AccessibilityValues &other) const noexcept
{
    return highContrast == other.highContrast && reducedMotion == other.reducedMotion
        && reducedTransparency == other.reducedTransparency
        && qFuzzyCompare(textScale, other.textScale);
}

} // namespace QindaQt::Apps::SettingsAccessibility
