// SPDX-License-Identifier: LGPL-3.0-or-later
#include "settings_value_normalizer_p.h"

#include <QMetaType>

#include <cmath>

namespace QindaQt::Settings::Internal {
namespace {

bool isNumericType(int typeId)
{
    switch (typeId) {
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Float:
    case QMetaType::Double:
        return true;
    default:
        return false;
    }
}

} // namespace

bool normalizeSettingValue(const SettingDefinition &definition,
                           const QVariant &input,
                           QVariant *normalized,
                           QString *message)
{
    const auto typeId = input.metaType().id();
    switch (definition.type) {
    case SettingValueType::Boolean:
        if (typeId != QMetaType::Bool) {
            *message = QStringLiteral("expected a boolean");
            return false;
        }
        *normalized = input.toBool();
        break;
    case SettingValueType::Integer: {
        if (!isNumericType(typeId)) {
            *message = QStringLiteral("expected an integer");
            return false;
        }
        bool converted = false;
        const double number = input.toDouble(&converted);
        constexpr double minimumInteger = -9223372036854775808.0;
        constexpr double maximumIntegerExclusive = 9223372036854775808.0;
        if (!converted || !std::isfinite(number) || std::trunc(number) != number
            || number < minimumInteger || number >= maximumIntegerExclusive) {
            *message = QStringLiteral("expected a finite integer");
            return false;
        }
        *normalized = QVariant::fromValue(static_cast<qint64>(number));
        break;
    }
    case SettingValueType::Number: {
        if (!isNumericType(typeId)) {
            *message = QStringLiteral("expected a number");
            return false;
        }
        bool converted = false;
        const double number = input.toDouble(&converted);
        if (!converted || !std::isfinite(number)) {
            *message = QStringLiteral("expected a finite number");
            return false;
        }
        *normalized = number;
        break;
    }
    case SettingValueType::String:
        if (typeId != QMetaType::QString) {
            *message = QStringLiteral("expected a string");
            return false;
        }
        *normalized = input.toString();
        break;
    case SettingValueType::StringList: {
        QStringList strings;
        if (typeId == QMetaType::QStringList) {
            strings = input.toStringList();
        } else if (typeId == QMetaType::QVariantList) {
            for (const auto &item : input.toList()) {
                if (item.metaType().id() != QMetaType::QString) {
                    *message = QStringLiteral("expected a list containing only strings");
                    return false;
                }
                strings.append(item.toString());
            }
        } else {
            *message = QStringLiteral("expected a string list");
            return false;
        }
        *normalized = strings;
        break;
    }
    case SettingValueType::Object:
        if (typeId != QMetaType::QVariantMap) {
            *message = QStringLiteral("expected an object");
            return false;
        }
        *normalized = input.toMap();
        break;
    }

    if (definition.nonEmpty) {
        const bool emptyString = definition.type == SettingValueType::String
            && normalized->toString().trimmed().isEmpty();
        const bool emptyList = definition.type == SettingValueType::StringList
            && normalized->toStringList().isEmpty();
        const bool emptyObject = definition.type == SettingValueType::Object
            && normalized->toMap().isEmpty();
        if (emptyString || emptyList || emptyObject) {
            *message = QStringLiteral("value must not be empty");
            return false;
        }
    }

    if (definition.type == SettingValueType::Integer
        || definition.type == SettingValueType::Number) {
        const double number = normalized->toDouble();
        if (definition.minimum.has_value() && number < *definition.minimum) {
            *message = QStringLiteral("value is below minimum %1").arg(*definition.minimum);
            return false;
        }
        if (definition.maximum.has_value() && number > *definition.maximum) {
            *message = QStringLiteral("value is above maximum %1").arg(*definition.maximum);
            return false;
        }
    }

    if (!definition.allowedValues.isEmpty()
        && !definition.allowedValues.contains(normalized->toString())) {
        *message = QStringLiteral("value is not one of: %1")
                       .arg(definition.allowedValues.join(QStringLiteral(", ")));
        return false;
    }
    return true;
}

} // namespace QindaQt::Settings::Internal
