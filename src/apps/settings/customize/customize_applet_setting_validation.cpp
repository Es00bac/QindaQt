// SPDX-License-Identifier: LGPL-3.0-or-later
#include "customize_applet_setting_validation.h"

#include <QJsonArray>
#include <QMetaType>

namespace QindaQt::Apps::SettingsCustomize {
namespace {

[[nodiscard]] bool isIntegralVariantType(QMetaType::Type type)
{
    return type == QMetaType::Int || type == QMetaType::UInt
        || type == QMetaType::LongLong || type == QMetaType::ULongLong;
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

[[nodiscard]] AppletSettingValidation validateBoundedInteger(
    const QJsonObject &propertySchema, const QString &key, const QVariant &value)
{
    const auto type = static_cast<QMetaType::Type>(value.typeId());
    qint64 candidate = 0;
    if (isIntegralVariantType(type)) {
        candidate = value.toLongLong();
    } else if (type == QMetaType::Double) {
        const double asDouble = value.toDouble();
        candidate = static_cast<qint64>(asDouble);
        if (static_cast<double>(candidate) != asDouble) {
            return failure(QStringLiteral("'%1' requires a whole number").arg(key));
        }
    } else {
        return failure(QStringLiteral("'%1' requires a whole number").arg(key));
    }
    const qint64 minimum =
        propertySchema.value(QStringLiteral("minimum")).toVariant().toLongLong();
    const qint64 maximum =
        propertySchema.value(QStringLiteral("maximum")).toVariant().toLongLong();
    if (candidate < minimum || candidate > maximum) {
        return failure(QStringLiteral("'%1' must be between %2 and %3")
                           .arg(key)
                           .arg(minimum)
                           .arg(maximum));
    }
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
        if (choice.toString() == candidate) {
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
    if (type == QLatin1String("integer")
        && propertySchema.contains(QStringLiteral("minimum"))
        && propertySchema.contains(QStringLiteral("maximum"))) {
        return AppletSettingFieldKind::BoundedInteger;
    }
    if (type == QLatin1String("string")
        && !propertySchema.value(QStringLiteral("enum")).toArray().isEmpty()) {
        return AppletSettingFieldKind::EnumChoice;
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
