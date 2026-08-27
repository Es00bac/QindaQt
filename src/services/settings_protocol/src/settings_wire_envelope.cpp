// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

#include <QDBusArgument>
#include <QDBusVariant>
#include <QSet>

#include <utility>

namespace QindaQt::Services::SettingsProtocol {
namespace {

bool exactType(const QVariant &value, int type)
{
    return value.isValid() && value.metaType().id() == type;
}

} // namespace

std::optional<QVariantList> decodeBoundedVariantList(const QVariant &value,
                                                      qsizetype maximumElements)
{
    if (maximumElements < 0) {
        return std::nullopt;
    }
    if (exactType(value, QMetaType::QVariantList)) {
        const QVariantList result = value.toList();
        return result.size() <= maximumElements ? std::optional<QVariantList>(result)
                                                : std::nullopt;
    }
    if (value.metaType() != QMetaType::fromType<QDBusArgument>()) {
        return std::nullopt;
    }
    const auto &argument = *static_cast<const QDBusArgument *>(value.constData());
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
        QVariant child = item.variant();
        item = QDBusVariant{};
        result.append(std::move(child));
    }
    argument.endArray();
    return result;
}

std::optional<QVariantMap> decodeBoundedVariantMap(const QVariant &value,
                                                   qsizetype maximumEntries)
{
    if (maximumEntries < 0) {
        return std::nullopt;
    }
    if (exactType(value, QMetaType::QVariantMap)) {
        const QVariantMap result = value.toMap();
        if (result.size() > maximumEntries) {
            return std::nullopt;
        }
        for (auto iterator = result.cbegin(); iterator != result.cend(); ++iterator) {
            if (!BoundedSettingsValueCodec::validateKey(iterator.key())) {
                return std::nullopt;
            }
        }
        return result;
    }
    if (value.metaType() != QMetaType::fromType<QDBusArgument>()) {
        return std::nullopt;
    }
    const auto &argument = *static_cast<const QDBusArgument *>(value.constData());
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
        if (result.contains(key) || !BoundedSettingsValueCodec::validateKey(key)) {
            return std::nullopt;
        }
        QVariant child = item.variant();
        item = QDBusVariant{};
        result.insert(std::move(key), std::move(child));
    }
    argument.endMap();
    return result;
}

std::optional<QStringList> decodeBoundedKeyList(const QVariant &value,
                                                qsizetype maximumElements)
{
    if (maximumElements < 0) {
        return std::nullopt;
    }
    QStringList result;
    if (exactType(value, QMetaType::QStringList)) {
        result = value.toStringList();
        if (result.size() > maximumElements) {
            return std::nullopt;
        }
    } else if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const auto &argument = *static_cast<const QDBusArgument *>(value.constData());
        if (argument.currentSignature() != QLatin1String("as")) {
            return std::nullopt;
        }
        argument.beginArray();
        while (!argument.atEnd()) {
            if (result.size() >= maximumElements) {
                return std::nullopt;
            }
            QString key;
            argument >> key;
            result.append(std::move(key));
        }
        argument.endArray();
    } else {
        return std::nullopt;
    }

    QSet<QString> unique;
    for (const auto &key : result) {
        if (!BoundedSettingsValueCodec::validateKey(key) || unique.contains(key)) {
            return std::nullopt;
        }
        unique.insert(key);
    }
    return result;
}

} // namespace QindaQt::Services::SettingsProtocol
