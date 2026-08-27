// SPDX-License-Identifier: LGPL-3.0-or-later
#include "settings_value_normalizer_p.h"

#include "canonical_json_value_p.h"

#include <QMetaType>

#include <cmath>
#include <utility>

namespace QindaQt::Settings::Internal {
namespace {

bool normalizedInteger(const QVariant &input, qint64 *output)
{
    QVariant normalized;
    QString ignored;
    if (!normalizeCanonicalJsonValue(input, &normalized, &ignored)
        || normalized.metaType().id() != QMetaType::LongLong) {
        return false;
    }
    *output = normalized.toLongLong();
    return true;
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
        qint64 integer = 0;
        if (!normalizedInteger(input, &integer)) {
            *message = QStringLiteral("expected a finite integer");
            return false;
        }
        *normalized = QVariant::fromValue(integer);
        break;
    }
    case SettingValueType::Number: {
        QVariant canonical;
        QString ignored;
        if (!normalizeCanonicalJsonValue(input, &canonical, &ignored)
            || (canonical.metaType().id() != QMetaType::LongLong
                && canonical.metaType().id() != QMetaType::Double)) {
            *message = QStringLiteral("expected a number");
            return false;
        }
        *normalized = canonical.toDouble();
        break;
    }
    case SettingValueType::String:
        if (typeId != QMetaType::QString) {
            *message = QStringLiteral("expected a string");
            return false;
        }
        if (!validateCanonicalJsonText(input.toString(), message)) {
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
                if (!validateCanonicalJsonText(item.toString(), message)) {
                    return false;
                }
                strings.append(item.toString());
            }
        } else {
            *message = QStringLiteral("expected a string list");
            return false;
        }
        for (const auto &string : std::as_const(strings)) {
            if (!validateCanonicalJsonText(string, message)) {
                return false;
            }
        }
        *normalized = strings;
        break;
    }
    case SettingValueType::Object:
        if (typeId != QMetaType::QVariantMap) {
            *message = QStringLiteral("expected an object");
            return false;
        }
        if (!normalizeCanonicalJsonValue(input, normalized, message)) {
            return false;
        }
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
