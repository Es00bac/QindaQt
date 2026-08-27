// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QDBusArgument>
#include <QDBusVariant>

namespace QindaQt::Services::SettingsProtocol {
namespace {

bool exactType(const QVariant &value, int type)
{
    return value.isValid() && value.metaType().id() == type;
}

std::optional<QVariant> decodeJsonValue(const QVariant &value, qsizetype depth)
{
    if (depth > WireContract::MaximumValueDepth) {
        return std::nullopt;
    }
    if (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
        return decodeJsonValue(qvariant_cast<QDBusVariant>(value).variant(), depth);
    }
    if (value.metaType() != QMetaType::fromType<QDBusArgument>()) {
        return value;
    }

    const QDBusArgument argument = qvariant_cast<QDBusArgument>(value);
    if (argument.currentSignature() == QLatin1String("av")) {
        QVariantList result;
        argument.beginArray();
        while (!argument.atEnd()) {
            if (result.size() >= WireContract::MaximumListEntries) {
                return std::nullopt;
            }
            QDBusVariant item;
            argument >> item;
            auto decoded = decodeJsonValue(item.variant(), depth + 1);
            if (!decoded.has_value()) {
                return std::nullopt;
            }
            result.append(*decoded);
        }
        argument.endArray();
        return QVariant::fromValue(result);
    }
    if (argument.currentSignature() == QLatin1String("a{sv}")) {
        QVariantMap result;
        argument.beginMap();
        while (!argument.atEnd()) {
            if (result.size() >= WireContract::MaximumMapEntries) {
                return std::nullopt;
            }
            QString key;
            QDBusVariant item;
            argument.beginMapEntry();
            argument >> key >> item;
            argument.endMapEntry();
            if (result.contains(key)) {
                return std::nullopt;
            }
            auto decoded = decodeJsonValue(item.variant(), depth + 1);
            if (!decoded.has_value()) {
                return std::nullopt;
            }
            result.insert(key, *decoded);
        }
        argument.endMap();
        return QVariant::fromValue(result);
    }
    return std::nullopt;
}

} // namespace

std::optional<QVariantList> decodeBoundedVariantList(const QVariant &value, qsizetype maximumElements)
{
    if (exactType(value, QMetaType::QVariantList)) {
        const QVariantList result = value.toList();
        return result.size() <= maximumElements ? std::optional<QVariantList>(result) : std::nullopt;
    }
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const QDBusArgument argument = qvariant_cast<QDBusArgument>(value);
        if (argument.currentSignature() != QLatin1String("av")) {
            return std::nullopt;
        }
        QVariantList result;
        argument.beginArray();
        while (!argument.atEnd()) {
            if (result.size() >= maximumElements) {
                return std::nullopt;
            }
            QDBusVariant item;
            argument >> item;
            result.append(item.variant());
        }
        argument.endArray();
        return result;
    }
    return std::nullopt;
}

std::optional<QVariant> decodeBoundedJsonValue(const QVariant &value, QString *error)
{
    auto decoded = decodeJsonValue(value, 1);
    if (!decoded.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("value has an unsupported or over-bounded D-Bus shape");
        }
        return std::nullopt;
    }
    if (!BoundedSettingsValueCodec::validateValue(*decoded, error)) {
        return std::nullopt;
    }
    return decoded;
}

std::optional<QVariantMap> decodeBoundedVariantMap(const QVariant &value, qsizetype maximumEntries)
{
    if (exactType(value, QMetaType::QVariantMap)) {
        const QVariantMap result = value.toMap();
        return result.size() <= maximumEntries ? std::optional<QVariantMap>(result) : std::nullopt;
    }
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const QDBusArgument argument = qvariant_cast<QDBusArgument>(value);
        if (argument.currentSignature() != QLatin1String("a{sv}")) {
            return std::nullopt;
        }
        QVariantMap result;
        argument.beginMap();
        while (!argument.atEnd()) {
            if (result.size() >= maximumEntries) {
                return std::nullopt;
            }
            QString key;
            QDBusVariant item;
            argument.beginMapEntry();
            argument >> key >> item;
            argument.endMapEntry();
            if (result.contains(key)) {
                return std::nullopt;
            }
            result.insert(std::move(key), item.variant());
        }
        argument.endMap();
        return result;
    }
    return std::nullopt;
}

} // namespace QindaQt::Services::SettingsProtocol
