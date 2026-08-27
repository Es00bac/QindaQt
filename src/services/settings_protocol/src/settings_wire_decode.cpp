// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QDBusArgument>
#include <QDBusVariant>
#include <QSet>

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

bool exactType(const QVariant &value, int type)
{
    return value.isValid() && value.metaType().id() == type;
}

bool wouldExceed(qsizetype current, qsizetype maximum, qsizetype addition)
{
    return current < 0 || maximum < 0 || addition < 0 || current > maximum
           || addition > maximum - current;
}

struct DecodeState final {
    BoundedSettingsValueCodec::Usage value;
    AggregateValueDecodeBudget aggregate;
};

bool chargeBytes(DecodeState &state, qsizetype bytes, QString *error)
{
    if (wouldExceed(state.value.bytes, WireContract::MaximumAggregateValueBytes, bytes)) {
        setError(error, QStringLiteral("value exceeds %1 aggregate bytes")
                            .arg(WireContract::MaximumAggregateValueBytes));
        return false;
    }
    if (wouldExceed(state.aggregate.bytes, state.aggregate.maximumBytes, bytes)) {
        setError(error, QStringLiteral("value envelope exceeds %1 aggregate bytes")
                            .arg(state.aggregate.maximumBytes));
        return false;
    }
    state.value.bytes += bytes;
    state.aggregate.bytes += bytes;
    return true;
}

bool chargeNode(DecodeState &state, qsizetype bytes, qsizetype depth, QString *error)
{
    if (depth < 1 || depth > WireContract::MaximumValueDepth) {
        setError(error, QStringLiteral("value exceeds depth %1")
                            .arg(WireContract::MaximumValueDepth));
        return false;
    }
    if (state.value.nodes >= WireContract::MaximumValueNodes) {
        setError(error, QStringLiteral("value exceeds %1 nodes")
                            .arg(WireContract::MaximumValueNodes));
        return false;
    }
    if (state.aggregate.nodes >= state.aggregate.maximumNodes) {
        setError(error, QStringLiteral("value envelope exceeds %1 aggregate nodes")
                            .arg(state.aggregate.maximumNodes));
        return false;
    }
    if (!chargeBytes(state, bytes, error)) {
        return false;
    }
    ++state.value.nodes;
    ++state.aggregate.nodes;
    state.value.maximumDepth = std::max(state.value.maximumDepth, depth);
    return true;
}

bool chargeKey(const QString &key, DecodeState &state, QString *error)
{
    if (!BoundedSettingsValueCodec::validateKey(key, error)) {
        return false;
    }
    return chargeBytes(state, key.toUtf8().size(), error);
}

bool chargeString(const QString &text, DecodeState &state, qsizetype depth, QString *error)
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
    return chargeNode(state, bytes, depth, error);
}

std::optional<QVariant> decodeJsonValue(const QVariant &value,
                                        qsizetype depth,
                                        DecodeState &state,
                                        QString *error);

std::optional<QVariant> decodeOpaqueArray(const QDBusArgument &argument,
                                          qsizetype depth,
                                          DecodeState &state,
                                          QString *error)
{
    if (!chargeNode(state, 8, depth, error)) {
        return std::nullopt;
    }
    QVariantList result;
    argument.beginArray();
    while (!argument.atEnd()) {
        if (result.size() >= WireContract::MaximumListEntries) {
            setError(error, QStringLiteral("list exceeds %1 entries at depth %2")
                                .arg(WireContract::MaximumListEntries)
                                .arg(depth));
            return std::nullopt;
        }
        QDBusVariant item;
        argument >> item;
        QVariant child = item.variant();
        item = QDBusVariant{};
        auto decoded = decodeJsonValue(child, depth + 1, state, error);
        if (!decoded.has_value()) {
            return std::nullopt;
        }
        // AGENT-GUARD: all recursive resource charges above must succeed before
        // an untrusted child is retained in the materialized result.
        result.append(std::move(*decoded));
    }
    argument.endArray();
    return QVariant::fromValue(result);
}

std::optional<QVariant> decodeOpaqueStringArray(const QDBusArgument &argument,
                                                qsizetype depth,
                                                DecodeState &state,
                                                QString *error)
{
    if (!chargeNode(state, 8, depth, error)) {
        return std::nullopt;
    }
    QStringList result;
    argument.beginArray();
    while (!argument.atEnd()) {
        if (result.size() >= WireContract::MaximumListEntries) {
            setError(error, QStringLiteral("list exceeds %1 entries at depth %2")
                                .arg(WireContract::MaximumListEntries)
                                .arg(depth));
            return std::nullopt;
        }
        QString item;
        argument >> item;
        if (!chargeString(item, state, depth + 1, error)) {
            return std::nullopt;
        }
        result.append(std::move(item));
    }
    argument.endArray();
    return QVariant::fromValue(result);
}

std::optional<QVariant> decodeOpaqueMap(const QDBusArgument &argument,
                                        qsizetype depth,
                                        DecodeState &state,
                                        QString *error)
{
    if (!chargeNode(state, 8, depth, error)) {
        return std::nullopt;
    }
    QVariantMap result;
    argument.beginMap();
    while (!argument.atEnd()) {
        if (result.size() >= WireContract::MaximumMapEntries) {
            setError(error, QStringLiteral("object exceeds %1 entries")
                                .arg(WireContract::MaximumMapEntries));
            return std::nullopt;
        }
        QString key;
        QDBusVariant item;
        argument.beginMapEntry();
        argument >> key >> item;
        argument.endMapEntry();
        if (result.contains(key)) {
            setError(error, QStringLiteral("object contains a duplicate key"));
            return std::nullopt;
        }
        if (!chargeKey(key, state, error)) {
            return std::nullopt;
        }
        QVariant child = item.variant();
        item = QDBusVariant{};
        auto decoded = decodeJsonValue(child, depth + 1, state, error);
        if (!decoded.has_value()) {
            return std::nullopt;
        }
        result.insert(std::move(key), std::move(*decoded));
    }
    argument.endMap();
    return QVariant::fromValue(result);
}

std::optional<QVariant> decodeOpaque(const QDBusArgument &argument,
                                     qsizetype depth,
                                     DecodeState &state,
                                     QString *error)
{
    const QString signature = argument.currentSignature();
    if (signature == QLatin1String("av")) {
        return decodeOpaqueArray(argument, depth, state, error);
    }
    if (signature == QLatin1String("as")) {
        return decodeOpaqueStringArray(argument, depth, state, error);
    }
    if (signature == QLatin1String("a{sv}")) {
        return decodeOpaqueMap(argument, depth, state, error);
    }
    setError(error, QStringLiteral("value has an unsupported D-Bus signature"));
    return std::nullopt;
}

std::optional<QVariant> decodeJsonValue(const QVariant &value,
                                        qsizetype depth,
                                        DecodeState &state,
                                        QString *error)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
        return decodeJsonValue(qvariant_cast<QDBusVariant>(value).variant(), depth, state, error);
    }
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        // QDBusArgument detaches its demarshalling cursor when copied. A
        // nested container copy then loses the parent's exact end boundary
        // and can walk later siblings. Read the argument held by this
        // QVariant in place; callers release the temporary QDBusVariant
        // wrapper before recursing so this is the sole live cursor.
        const auto *argument = static_cast<const QDBusArgument *>(value.constData());
        return decodeOpaque(*argument, depth, state, error);
    }
    if (!value.isValid() || value.isNull()) {
        return chargeNode(state, 1, depth, error) ? std::optional<QVariant>(QVariant{})
                                                  : std::nullopt;
    }
    switch (value.metaType().id()) {
    case QMetaType::Bool:
        return chargeNode(state, 1, depth, error) ? std::optional<QVariant>(value)
                                                  : std::nullopt;
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        return chargeNode(state, 8, depth, error) ? std::optional<QVariant>(value)
                                                  : std::nullopt;
    case QMetaType::Double:
        if (!std::isfinite(value.toDouble())) {
            setError(error, QStringLiteral("number is not finite"));
            return std::nullopt;
        }
        return chargeNode(state, 8, depth, error) ? std::optional<QVariant>(value)
                                                  : std::nullopt;
    case QMetaType::QString: {
        const QString text = value.toString();
        return chargeString(text, state, depth, error)
                   ? std::optional<QVariant>(QVariant::fromValue(text)) : std::nullopt;
    }
    case QMetaType::QStringList: {
        const QStringList list = value.toStringList();
        if (list.size() > WireContract::MaximumListEntries) {
            setError(error, QStringLiteral("list exceeds %1 entries")
                                .arg(WireContract::MaximumListEntries));
            return std::nullopt;
        }
        if (!chargeNode(state, 8, depth, error)) {
            return std::nullopt;
        }
        QStringList result;
        for (const auto &entry : list) {
            if (!chargeString(entry, state, depth + 1, error)) {
                return std::nullopt;
            }
            result.append(entry);
        }
        return QVariant::fromValue(result);
    }
    case QMetaType::QVariantList: {
        const QVariantList list = value.toList();
        if (list.size() > WireContract::MaximumListEntries) {
            setError(error, QStringLiteral("list exceeds %1 entries")
                                .arg(WireContract::MaximumListEntries));
            return std::nullopt;
        }
        if (!chargeNode(state, 8, depth, error)) {
            return std::nullopt;
        }
        QVariantList result;
        for (const auto &entry : list) {
            auto decoded = decodeJsonValue(entry, depth + 1, state, error);
            if (!decoded.has_value()) {
                return std::nullopt;
            }
            result.append(std::move(*decoded));
        }
        return QVariant::fromValue(result);
    }
    case QMetaType::QVariantMap: {
        const QVariantMap map = value.toMap();
        if (map.size() > WireContract::MaximumMapEntries) {
            setError(error, QStringLiteral("object exceeds %1 entries")
                                .arg(WireContract::MaximumMapEntries));
            return std::nullopt;
        }
        if (!chargeNode(state, 8, depth, error)) {
            return std::nullopt;
        }
        QVariantMap result;
        for (auto iterator = map.cbegin(); iterator != map.cend(); ++iterator) {
            if (!chargeKey(iterator.key(), state, error)) {
                return std::nullopt;
            }
            auto decoded = decodeJsonValue(iterator.value(), depth + 1, state, error);
            if (!decoded.has_value()) {
                return std::nullopt;
            }
            result.insert(iterator.key(), std::move(*decoded));
        }
        return QVariant::fromValue(result);
    }
    default:
        setError(error, QStringLiteral("value type is not JSON-native"));
        return std::nullopt;
    }
}

std::optional<QVariant> decodeWithBudget(const QVariant &value,
                                         AggregateValueDecodeBudget &aggregate,
                                         QString *error,
                                         BoundedSettingsValueCodec::Usage *usage)
{
    if (aggregate.maximumBytes < 0 || aggregate.maximumNodes < 0
        || aggregate.bytes < 0 || aggregate.nodes < 0
        || aggregate.bytes > aggregate.maximumBytes
        || aggregate.nodes > aggregate.maximumNodes) {
        setError(error, QStringLiteral("aggregate decode budget is invalid"));
        return std::nullopt;
    }
    DecodeState state{{}, aggregate};
    auto result = decodeJsonValue(value, 1, state, error);
    if (usage != nullptr) {
        *usage = state.value;
    }
    if (!result.has_value()) {
        return std::nullopt;
    }
    aggregate = state.aggregate;
    return result;
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

std::optional<QVariant> decodeBoundedJsonValue(
    const QVariant &value,
    QString *error,
    BoundedSettingsValueCodec::Usage *usage)
{
    AggregateValueDecodeBudget aggregate{WireContract::MaximumAggregateValueBytes,
                                         WireContract::MaximumValueNodes};
    return decodeWithBudget(value, aggregate, error, usage);
}

std::optional<QVariant> decodeBoundedJsonValue(
    const QVariant &value,
    AggregateValueDecodeBudget &aggregate,
    QString *error,
    BoundedSettingsValueCodec::Usage *usage)
{
    return decodeWithBudget(value, aggregate, error, usage);
}

} // namespace QindaQt::Services::SettingsProtocol
