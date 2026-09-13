// SPDX-License-Identifier: LGPL-3.0-or-later
#include "customize_applet_setting_validation.h"

#include <QJsonArray>
#include <QMetaType>

#include <cmath>
#include <limits>
#include <optional>

namespace QindaQt::Apps::SettingsCustomize {
namespace {

[[nodiscard]] bool isIntegralVariantType(QMetaType::Type type)
{
    return type == QMetaType::Int || type == QMetaType::UInt
        || type == QMetaType::LongLong || type == QMetaType::ULongLong;
}

// A schema `minimum`/`maximum` is a usable integer bound only if it is a
// finite JSON number with no fractional part that fits in the storage width
// this validator commits accepted values as (int, matching Profiles::
// AppletSpec's other integer fields). A null, missing, non-numeric,
// fractional, infinite, or out-of-width bound makes the whole field
// Unsupported rather than silently clamping or narrowing later (review
// finding 2).
[[nodiscard]] std::optional<qint64> integralJsonBound(const QJsonValue &value)
{
    if (value.type() != QJsonValue::Double) {
        return std::nullopt;
    }
    const double raw = value.toDouble();
    if (!std::isfinite(raw)) {
        return std::nullopt;
    }
    if (raw < static_cast<double>(std::numeric_limits<int>::min())
        || raw > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    const auto candidate = static_cast<qint64>(raw);
    if (static_cast<double>(candidate) != raw) {
        return std::nullopt;
    }
    return candidate;
}

[[nodiscard]] AppletSettingValidation failure(const QString &message)
{
    return {{}, message};
}

[[nodiscard]] AppletSettingValidation validateBoolean(const QString &key,
                                                      const QVariant &value)
{
    if (static_cast<QMetaType::Type>(value.typeId()) != QMetaType::Bool) {
        return failure(QStringLiteral("'%1' requires a true/false value").arg(key));
    }
    return {value, {}};
}

// AGENT-GUARD: only called once appletSettingFieldKind() has already proven
// `minimum`/`maximum` are both finite, integral, ordered, int-representable
// bounds. Every accepted candidate is therefore checked against an
// int-representable range before the final QVariant(int) conversion, so
// that conversion can never narrow a value the manifest itself declared
// in-bounds (review finding 1: a schema bound like [0, 2147483648] no
// longer classifies as BoundedInteger at all).
[[nodiscard]] AppletSettingValidation validateBoundedInteger(
    const QJsonObject &propertySchema, const QString &key, const QVariant &value)
{
    const auto minimum = integralJsonBound(propertySchema.value(QStringLiteral("minimum")));
    const auto maximum = integralJsonBound(propertySchema.value(QStringLiteral("maximum")));
    if (!minimum.has_value() || !maximum.has_value() || *minimum > *maximum) {
        return failure(QStringLiteral("'%1' has no usable declared bounds").arg(key));
    }

    const auto type = static_cast<QMetaType::Type>(value.typeId());
    qint64 candidate = 0;
    if (isIntegralVariantType(type)) {
        candidate = value.toLongLong();
    } else if (type == QMetaType::Double) {
        const double asDouble = value.toDouble();
        if (!std::isfinite(asDouble)) {
            return failure(QStringLiteral("'%1' requires a whole number").arg(key));
        }
        candidate = static_cast<qint64>(asDouble);
        if (static_cast<double>(candidate) != asDouble) {
            return failure(QStringLiteral("'%1' requires a whole number").arg(key));
        }
    } else {
        return failure(QStringLiteral("'%1' requires a whole number").arg(key));
    }
    if (candidate < *minimum || candidate > *maximum) {
        return failure(QStringLiteral("'%1' must be between %2 and %3")
                           .arg(key)
                           .arg(*minimum)
                           .arg(*maximum));
    }
    // Safe: candidate is bounded by *minimum/*maximum, both already proven
    // int-representable above.
    return {QVariant(static_cast<int>(candidate)), {}};
}

[[nodiscard]] AppletSettingValidation validateEnumChoice(
    const QJsonObject &propertySchema, const QString &key, const QVariant &value)
{
    if (static_cast<QMetaType::Type>(value.typeId()) != QMetaType::QString) {
        return failure(
            QStringLiteral("'%1' requires one of its listed choices").arg(key));
    }
    const QString candidate = value.toString();
    const QJsonArray choices = propertySchema.value(QStringLiteral("enum")).toArray();
    for (const auto &choice : choices) {
        // AGENT-GUARD: appletSettingFieldKind() already proved every member
        // is a JSON string; toString() here is a lookup, not a coercion.
        if (choice.type() == QJsonValue::String && choice.toString() == candidate) {
            return {value, {}};
        }
    }
    return failure(
        QStringLiteral("'%1' must be one of its listed choices").arg(key));
}

} // namespace

AppletSettingFieldKind appletSettingFieldKind(const QJsonObject &propertySchema)
{
    const QString type = propertySchema.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("boolean")) {
        return AppletSettingFieldKind::Boolean;
    }
    if (type == QLatin1String("integer")) {
        const auto minimum =
            integralJsonBound(propertySchema.value(QStringLiteral("minimum")));
        const auto maximum =
            integralJsonBound(propertySchema.value(QStringLiteral("maximum")));
        if (minimum.has_value() && maximum.has_value() && *minimum <= *maximum) {
            return AppletSettingFieldKind::BoundedInteger;
        }
        return AppletSettingFieldKind::Unsupported;
    }
    if (type == QLatin1String("string")) {
        const QJsonArray choices =
            propertySchema.value(QStringLiteral("enum")).toArray();
        if (!choices.isEmpty()) {
            bool everyChoiceIsAString = true;
            for (const auto &choice : choices) {
                if (choice.type() != QJsonValue::String) {
                    everyChoiceIsAString = false;
                    break;
                }
            }
            if (everyChoiceIsAString) {
                return AppletSettingFieldKind::EnumChoice;
            }
        }
        return AppletSettingFieldKind::Unsupported;
    }
    return AppletSettingFieldKind::Unsupported;
}

AppletSettingValidation validateAppletSettingValue(
    const QJsonObject &settingsSchema, const QString &key, const QVariant &value)
{
    const QJsonObject properties =
        settingsSchema.value(QStringLiteral("properties")).toObject();
    if (!properties.contains(key)) {
        return failure(QStringLiteral("'%1' is not a declared setting").arg(key));
    }
    const QJsonObject propertySchema = properties.value(key).toObject();
    switch (appletSettingFieldKind(propertySchema)) {
    case AppletSettingFieldKind::Boolean:
        return validateBoolean(key, value);
    case AppletSettingFieldKind::BoundedInteger:
        return validateBoundedInteger(propertySchema, key, value);
    case AppletSettingFieldKind::EnumChoice:
        return validateEnumChoice(propertySchema, key, value);
    case AppletSettingFieldKind::Unsupported:
        break;
    }
    return failure(QStringLiteral("'%1' cannot be edited from Customize").arg(key));
}

QVariant appletSettingSchemaDefault(const QJsonObject &settingsSchema, const QString &key)
{
    const QJsonObject properties =
        settingsSchema.value(QStringLiteral("properties")).toObject();
    return properties.value(key).toObject().value(QStringLiteral("default")).toVariant();
}

} // namespace QindaQt::Apps::SettingsCustomize
