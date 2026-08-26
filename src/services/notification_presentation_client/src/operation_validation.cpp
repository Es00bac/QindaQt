// SPDX-License-Identifier: LGPL-3.0-or-later
#include "operation_validation.h"

#include <QMetaType>
#include <QSet>

namespace QindaQt::Services::NotificationPresentationClient::Private {
namespace {

constexpr qsizetype MaximumPresentedErrorCharacters = 512;

QString sanitizedErrorText(const QString &message)
{
    QString result;
    result.reserve(message.size());
    for (qsizetype index = 0; index < message.size(); ++index) {
        const QChar character = message.at(index);
        if (character == QChar::Null || character.isLowSurrogate()) {
            result.append(QChar(0xfffd));
        } else if (character.isHighSurrogate()) {
            if (index + 1 < message.size() &&
                message.at(index + 1).isLowSurrogate()) {
                result.append(character);
                result.append(message.at(++index));
            } else {
                result.append(QChar(0xfffd));
            }
        } else {
            result.append(character);
        }
    }
    return result;
}

} // namespace

bool validBoundedText(const QString &value, qsizetype maximumBytes)
{
    if (value.isEmpty() || value.contains(QChar::Null) ||
        value.toUtf8().size() > maximumBytes) {
        return false;
    }
    for (qsizetype index = 0; index < value.size(); ++index) {
        if (value.at(index).isHighSurrogate()) {
            if (++index >= value.size() || !value.at(index).isLowSurrogate()) {
                return false;
            }
        } else if (value.at(index).isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

bool validOperationResult(const QVariantMap &result, quint32 expectedId,
                          quint64 minimumRevisionBefore,
                          bool unchangedRevisionAllowed, quint64 *revisionAfter)
{
    static const QSet<QString> Keys = {
        QStringLiteral("status"), QStringLiteral("revisionBefore"),
        QStringLiteral("revisionAfter"), QStringLiteral("notificationId")};
    if (QSet<QString>(result.keyBegin(), result.keyEnd()) != Keys ||
        result.value(QStringLiteral("status")).metaType().id() !=
            QMetaType::QString ||
        result.value(QStringLiteral("status")).toString() !=
            QLatin1String("applied") ||
        result.value(QStringLiteral("revisionBefore")).metaType().id() !=
            QMetaType::ULongLong ||
        result.value(QStringLiteral("revisionAfter")).metaType().id() !=
            QMetaType::ULongLong ||
        result.value(QStringLiteral("notificationId")).metaType().id() !=
            QMetaType::UInt ||
        result.value(QStringLiteral("notificationId")).toUInt() != expectedId) {
        return false;
    }
    const quint64 before =
        result.value(QStringLiteral("revisionBefore")).toULongLong();
    *revisionAfter =
        result.value(QStringLiteral("revisionAfter")).toULongLong();
    // AGENT-CONTRACT: NotificationService preserves a revision only when an
    // action targets a resident notification; dismiss and non-resident action
    // replies must prove the corresponding model mutation.
    return before >= minimumRevisionBefore && *revisionAfter >= before &&
        (unchangedRevisionAllowed || *revisionAfter > before);
}

QString normalizedOperationError(QString message, const QString &fallback)
{
    message = message.trimmed();
    if (message.isEmpty()) {
        message = fallback;
    }
    message = sanitizedErrorText(message);
    if (message.size() <= MaximumPresentedErrorCharacters) {
        return message;
    }
    message.truncate(MaximumPresentedErrorCharacters - 1);
    if (!message.isEmpty() && message.back().isHighSurrogate()) {
        message.chop(1);
    }
    message.append(QChar(0x2026));
    return message;
}

} // namespace QindaQt::Services::NotificationPresentationClient::Private
