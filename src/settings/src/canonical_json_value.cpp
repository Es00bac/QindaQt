// SPDX-License-Identifier: LGPL-3.0-or-later
#include "canonical_json_value_p.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::Settings::Internal {
namespace {

void setError(QString *message, QString text)
{
    if (message != nullptr) {
        *message = std::move(text);
    }
}

bool signedInteger(const QVariant &input, qint64 *output)
{
    switch (input.metaType().id()) {
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::Short:
    case QMetaType::Int:
    case QMetaType::Long:
    case QMetaType::LongLong: {
        bool ok = false;
        const qint64 value = input.toLongLong(&ok);
        if (ok) {
            *output = value;
        }
        return ok;
    }
    default:
        return false;
    }
}

bool unsignedInteger(const QVariant &input, qint64 *output, bool *recognized)
{
    switch (input.metaType().id()) {
    case QMetaType::UChar:
    case QMetaType::UShort:
    case QMetaType::UInt:
    case QMetaType::ULong:
    case QMetaType::ULongLong: {
        *recognized = true;
        bool ok = false;
        const quint64 value = input.toULongLong(&ok);
        if (!ok || value > quint64(std::numeric_limits<qint64>::max())) {
            return false;
        }
        *output = static_cast<qint64>(value);
        return true;
    }
    default:
        *recognized = false;
        return false;
    }
}

bool floatingNumber(const QVariant &input, double *output)
{
    if (input.metaType().id() != QMetaType::Float
        && input.metaType().id() != QMetaType::Double) {
        return false;
    }
    bool ok = false;
    const double number = input.toDouble(&ok);
    if (!ok || !std::isfinite(number)) {
        return false;
    }
    *output = number;
    return true;
}

bool normalizeList(const QVariantList &input, QVariant *normalized, QString *message)
{
    QVariantList result;
    result.reserve(input.size());
    for (const auto &entry : input) {
        QVariant child;
        if (!normalizeCanonicalJsonValue(entry, &child, message)) {
            return false;
        }
        result.append(std::move(child));
    }
    *normalized = std::move(result);
    return true;
}

} // namespace

bool validateCanonicalJsonText(const QString &text, QString *message)
{
    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (character.isNull()) {
            setError(message, QStringLiteral("text contains an embedded NUL"));
            return false;
        }
        if (character.isHighSurrogate()) {
            if (index + 1 >= text.size() || !text.at(index + 1).isLowSurrogate()) {
                setError(message, QStringLiteral("text contains ill-formed UTF-16"));
                return false;
            }
            ++index;
        } else if (character.isLowSurrogate()) {
            setError(message, QStringLiteral("text contains ill-formed UTF-16"));
            return false;
        }
    }
    return true;
}

bool normalizeCanonicalJsonValue(const QVariant &input,
                                 QVariant *normalized,
                                 QString *message)
{
    if (normalized == nullptr) {
        setError(message, QStringLiteral("normalization output is missing"));
        return false;
    }
    const int type = input.metaType().id();
    if (!input.isValid()) {
        setError(message, QStringLiteral("invalid QVariant is not JSON null"));
        return false;
    }
    if (type == QMetaType::Nullptr) {
        *normalized = QVariant::fromValue(nullptr);
        return true;
    }
    if (type == QMetaType::Bool) {
        *normalized = input.toBool();
        return true;
    }

    qint64 integer = 0;
    if (signedInteger(input, &integer)) {
        *normalized = QVariant::fromValue(integer);
        return true;
    }
    bool recognizedUnsigned = false;
    if (unsignedInteger(input, &integer, &recognizedUnsigned)) {
        *normalized = QVariant::fromValue(integer);
        return true;
    }
    if (recognizedUnsigned) {
        setError(message, QStringLiteral("integer exceeds the canonical signed 64-bit range"));
        return false;
    }

    double number = 0.0;
    if (floatingNumber(input, &number)) {
        constexpr double minimumInteger = -9223372036854775808.0;
        constexpr double maximumIntegerExclusive = 9223372036854775808.0;
        if (std::trunc(number) == number && number >= minimumInteger
            && number < maximumIntegerExclusive) {
            *normalized = QVariant::fromValue(static_cast<qint64>(number));
        } else {
            *normalized = number;
        }
        return true;
    }
    if (type == QMetaType::Float || type == QMetaType::Double) {
        setError(message, QStringLiteral("number is not finite"));
        return false;
    }
    if (type == QMetaType::QString) {
        if (!validateCanonicalJsonText(input.toString(), message)) {
            return false;
        }
        *normalized = input.toString();
        return true;
    }
    if (type == QMetaType::QStringList) {
        QVariantList list;
        const QStringList strings = input.toStringList();
        list.reserve(strings.size());
        for (const auto &entry : strings) {
            list.append(entry);
        }
        return normalizeList(list, normalized, message);
    }
    if (type == QMetaType::QVariantList) {
        return normalizeList(input.toList(), normalized, message);
    }
    if (type == QMetaType::QVariantMap) {
        QVariantMap result;
        const QVariantMap map = input.toMap();
        for (auto iterator = map.cbegin(); iterator != map.cend(); ++iterator) {
            if (iterator.key().isEmpty()
                || !validateCanonicalJsonText(iterator.key(), message)) {
                if (iterator.key().isEmpty()) {
                    setError(message, QStringLiteral("object key must not be empty"));
                }
                return false;
            }
            QVariant child;
            if (!normalizeCanonicalJsonValue(iterator.value(), &child, message)) {
                return false;
            }
            result.insert(iterator.key(), std::move(child));
        }
        *normalized = std::move(result);
        return true;
    }
    setError(message, QStringLiteral("value type is not JSON-native"));
    return false;
}

std::optional<QJsonValue> encodeCanonicalJsonValue(const QVariant &value,
                                                   QString *message)
{
    switch (value.metaType().id()) {
    case QMetaType::Nullptr:
        return QJsonValue(QJsonValue::Null);
    case QMetaType::Bool:
        return QJsonValue(value.toBool());
    case QMetaType::LongLong:
        return QJsonValue(value.toLongLong());
    case QMetaType::Double:
        if (std::isfinite(value.toDouble())) {
            return QJsonValue(value.toDouble());
        }
        break;
    case QMetaType::QString:
        return QJsonValue(value.toString());
    case QMetaType::QVariantList: {
        QJsonArray result;
        for (const auto &entry : value.toList()) {
            auto encoded = encodeCanonicalJsonValue(entry, message);
            if (!encoded.has_value()) {
                return std::nullopt;
            }
            result.append(std::move(*encoded));
        }
        return QJsonValue(std::move(result));
    }
    case QMetaType::QVariantMap: {
        QJsonObject result;
        const QVariantMap map = value.toMap();
        for (auto iterator = map.cbegin(); iterator != map.cend(); ++iterator) {
            auto encoded = encodeCanonicalJsonValue(iterator.value(), message);
            if (!encoded.has_value()) {
                return std::nullopt;
            }
            result.insert(iterator.key(), std::move(*encoded));
        }
        return QJsonValue(std::move(result));
    }
    default:
        break;
    }
    setError(message, QStringLiteral("value is outside the canonical JSON domain"));
    return std::nullopt;
}

} // namespace QindaQt::Settings::Internal
