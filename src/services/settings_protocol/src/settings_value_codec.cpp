// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_value_codec.h"

#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::Services::SettingsProtocol {
namespace {

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

bool addUsage(BoundedSettingsValueCodec::Usage &usage,
              qsizetype bytes,
              qsizetype depth,
              QString *error)
{
    if (bytes < 0 || usage.bytes > WireContract::MaximumAggregateValueBytes - bytes) {
        setError(error, QStringLiteral("value exceeds %1 aggregate bytes")
                            .arg(WireContract::MaximumAggregateValueBytes));
        return false;
    }
    ++usage.nodes;
    usage.bytes += bytes;
    usage.maximumDepth = std::max(usage.maximumDepth, depth);
    if (usage.nodes > WireContract::MaximumValueNodes) {
        setError(error, QStringLiteral("value exceeds %1 nodes")
                            .arg(WireContract::MaximumValueNodes));
        return false;
    }
    if (depth > WireContract::MaximumValueDepth) {
        setError(error, QStringLiteral("value exceeds depth %1")
                            .arg(WireContract::MaximumValueDepth));
        return false;
    }
    return true;
}

bool boundedString(const QString &text,
                   BoundedSettingsValueCodec::Usage &usage,
                   qsizetype depth,
                   QString *error)
{
    if (text.contains(QChar(u'\0'))) {
        setError(error, QStringLiteral("string contains an embedded NUL"));
        return false;
    }
    const qsizetype bytes = text.toUtf8().size();
    if (bytes > WireContract::MaximumStringValueBytes) {
        setError(error, QStringLiteral("string exceeds %1 UTF-8 bytes")
                            .arg(WireContract::MaximumStringValueBytes));
        return false;
    }
    return addUsage(usage, bytes, depth, error);
}

bool validate(const QVariant &value,
              BoundedSettingsValueCodec::Usage &usage,
              qsizetype depth,
              QString *error)
{
    if (!value.isValid() || value.isNull()) {
        return addUsage(usage, 1, depth, error);
    }
    switch (value.metaType().id()) {
    case QMetaType::Bool:
        return addUsage(usage, 1, depth, error);
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        return addUsage(usage, 8, depth, error);
    case QMetaType::Double:
        if (!std::isfinite(value.toDouble())) {
            setError(error, QStringLiteral("number is not finite"));
            return false;
        }
        return addUsage(usage, 8, depth, error);
    case QMetaType::QString:
        return boundedString(value.toString(), usage, depth, error);
    case QMetaType::QStringList: {
        const auto list = value.toStringList();
        if (list.size() > WireContract::MaximumListEntries) {
            setError(error, QStringLiteral("list exceeds %1 entries")
                                .arg(WireContract::MaximumListEntries));
            return false;
        }
        if (!addUsage(usage, 8, depth, error)) {
            return false;
        }
        for (const auto &entry : list) {
            if (!boundedString(entry, usage, depth + 1, error)) {
                return false;
            }
        }
        return true;
    }
    case QMetaType::QVariantList: {
        const auto list = value.toList();
        if (list.size() > WireContract::MaximumListEntries) {
            setError(error, QStringLiteral("list exceeds %1 entries")
                                .arg(WireContract::MaximumListEntries));
            return false;
        }
        if (!addUsage(usage, 8, depth, error)) {
            return false;
        }
        for (const auto &entry : list) {
            if (!validate(entry, usage, depth + 1, error)) {
                return false;
            }
        }
        return true;
    }
    case QMetaType::QVariantMap: {
        const auto map = value.toMap();
        if (map.size() > WireContract::MaximumMapEntries) {
            setError(error, QStringLiteral("object exceeds %1 entries")
                                .arg(WireContract::MaximumMapEntries));
            return false;
        }
        if (!addUsage(usage, 8, depth, error)) {
            return false;
        }
        for (auto iterator = map.cbegin(); iterator != map.cend(); ++iterator) {
            if (!BoundedSettingsValueCodec::validateKey(iterator.key(), error)) {
                return false;
            }
            const qsizetype keyBytes = iterator.key().toUtf8().size();
            if (usage.bytes > WireContract::MaximumAggregateValueBytes - keyBytes) {
                setError(error, QStringLiteral("value exceeds %1 aggregate bytes")
                                    .arg(WireContract::MaximumAggregateValueBytes));
                return false;
            }
            usage.bytes += keyBytes;
            if (!validate(iterator.value(), usage, depth + 1, error)) {
                return false;
            }
        }
        return true;
    }
    default:
        setError(error, QStringLiteral("value type is not JSON-native"));
        return false;
    }
}

} // namespace

bool BoundedSettingsValueCodec::validateKey(const QString &key, QString *error)
{
    if (key.isEmpty() || key.contains(QChar(u'\0'))) {
        setError(error, QStringLiteral("key must be non-empty and contain no NUL"));
        return false;
    }
    if (key.toUtf8().size() > WireContract::MaximumKeyBytes) {
        setError(error, QStringLiteral("key exceeds %1 UTF-8 bytes")
                            .arg(WireContract::MaximumKeyBytes));
        return false;
    }
    return true;
}

bool BoundedSettingsValueCodec::validateValue(const QVariant &value,
                                               QString *error,
                                               Usage *usage)
{
    Usage local;
    const bool valid = validate(value, local, 1, error);
    if (usage != nullptr) {
        *usage = local;
    }
    return valid;
}

} // namespace QindaQt::Services::SettingsProtocol
