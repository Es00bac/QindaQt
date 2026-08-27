// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_wire_encode.h"

#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include "settings_wire_decode_p.h"

#include <QDBusSignature>
#include <QMetaType>
#include <QSet>

#include <utility>

namespace QindaQt::Services::SettingsProtocol {
namespace {

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

QVariant wireValue(const QVariant &canonical)
{
    if (canonical.metaType().id() == QMetaType::Nullptr) {
        return QVariant::fromValue(
            QDBusSignature(QString::fromLatin1(WireContract::JsonNullWireSignature)));
    }
    if (canonical.metaType().id() == QMetaType::QVariantList) {
        QVariantList result;
        const QVariantList list = canonical.toList();
        result.reserve(list.size());
        for (const auto &entry : list) {
            result.append(wireValue(entry));
        }
        return result;
    }
    if (canonical.metaType().id() == QMetaType::QVariantMap) {
        QVariantMap result;
        const QVariantMap map = canonical.toMap();
        for (auto iterator = map.cbegin(); iterator != map.cend(); ++iterator) {
            result.insert(iterator.key(), wireValue(iterator.value()));
        }
        return result;
    }
    return canonical;
}

std::optional<QVariant> encode(const QVariant &value,
                               AggregateValueDecodeBudget &aggregate,
                               QString *error,
                               BoundedSettingsValueCodec::Usage *usage)
{
    auto canonical = Private::normalizeBoundedJsonValueForWireEncoding(
        value, aggregate, error, usage);
    if (!canonical.has_value()) {
        return std::nullopt;
    }
    return wireValue(*canonical);
}

} // namespace

std::optional<QVariant> encodeBoundedJsonValueForWire(
    const QVariant &value,
    QString *error,
    BoundedSettingsValueCodec::Usage *usage)
{
    AggregateValueDecodeBudget aggregate{WireContract::MaximumAggregateValueBytes,
                                         WireContract::MaximumValueNodes};
    return encode(value, aggregate, error, usage);
}

std::optional<QVariant> encodeBoundedJsonValueForWire(
    const QVariant &value,
    AggregateValueDecodeBudget &aggregate,
    QString *error,
    BoundedSettingsValueCodec::Usage *usage)
{
    return encode(value, aggregate, error, usage);
}

std::optional<QVariantList> encodeBoundedOperationsForWire(
    const QVariantList &operations,
    QString *error)
{
    if (operations.isEmpty()
        || operations.size() > WireContract::MaximumOperationsPerTransaction) {
        setError(error, QStringLiteral("transaction has an invalid operation count"));
        return std::nullopt;
    }
    AggregateValueDecodeBudget aggregate{WireContract::MaximumTransactionValueBytes,
                                         WireContract::MaximumTransactionValueNodes};
    QVariantList result;
    result.reserve(operations.size());
    QSet<QString> keys;
    for (const auto &entry : operations) {
        const auto operation = decodeBoundedVariantMap(
            entry, WireContract::OperationFieldCount);
        if (!operation.has_value()) {
            setError(error, QStringLiteral("operation entry is not a bounded object"));
            return std::nullopt;
        }
        const QVariant keyField = operation->value(
            QLatin1StringView(WireContract::FieldKey));
        const QVariant kindField = operation->value(
            QLatin1StringView(WireContract::FieldKind));
        if (keyField.metaType().id() != QMetaType::QString
            || kindField.metaType().id() != QMetaType::QString) {
            setError(error, QStringLiteral("operation key and kind must be strings"));
            return std::nullopt;
        }
        const QString key = keyField.toString();
        const QString kind = kindField.toString();
        if (!BoundedSettingsValueCodec::validateKey(key, error) || keys.contains(key)) {
            if (keys.contains(key)) {
                setError(error, QStringLiteral("transaction contains a duplicate key"));
            }
            return std::nullopt;
        }
        keys.insert(key);
        QVariantMap encoded{{QLatin1StringView(WireContract::FieldKey), key},
                            {QLatin1StringView(WireContract::FieldKind), kind}};
        if (kind == QLatin1StringView(WireContract::OperationKindRemove)) {
            if (operation->size() != 2
                || operation->contains(QLatin1StringView(WireContract::FieldValue))) {
                setError(error, QStringLiteral("remove operation must not contain a value"));
                return std::nullopt;
            }
        } else if (kind == QLatin1StringView(WireContract::OperationKindSet)) {
            if (operation->size() != WireContract::OperationFieldCount
                || !operation->contains(QLatin1StringView(WireContract::FieldValue))) {
                setError(error, QStringLiteral("set operation requires a value"));
                return std::nullopt;
            }
            auto wireValue = encodeBoundedJsonValueForWire(
                operation->value(QLatin1StringView(WireContract::FieldValue)),
                aggregate, error);
            if (!wireValue.has_value()) {
                return std::nullopt;
            }
            encoded.insert(QLatin1StringView(WireContract::FieldValue),
                           std::move(*wireValue));
        } else {
            setError(error, QStringLiteral("operation kind must be set or remove"));
            return std::nullopt;
        }
        result.append(std::move(encoded));
    }
    return result;
}

} // namespace QindaQt::Services::SettingsProtocol
