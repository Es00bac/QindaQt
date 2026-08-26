// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariantMap>
#include <QtTypes>

namespace QindaQt::Services::NotificationPresentationClient::Private {

[[nodiscard]] bool validBoundedText(const QString &value,
                                    qsizetype maximumBytes);
[[nodiscard]] bool validOperationResult(const QVariantMap &result,
                                        quint32 expectedId,
                                        quint64 minimumRevisionBefore,
                                        bool unchangedRevisionAllowed,
                                        quint64 *revisionAfter);
[[nodiscard]] QString normalizedOperationError(QString message,
                                               const QString &fallback);

} // namespace QindaQt::Services::NotificationPresentationClient::Private
