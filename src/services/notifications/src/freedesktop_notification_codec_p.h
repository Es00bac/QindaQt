// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notifications/notification_types.h"

#include <QDBusArgument>
#include <QVariantMap>

namespace QindaQt::Services::Notifications::Private {

struct FreedesktopImageData final {
    int width = 0;
    int height = 0;
    int rowStride = 0;
    bool hasAlpha = false;
    int bitsPerSample = 0;
    int channels = 0;
    QByteArray pixels;
};

QDBusArgument &operator<<(QDBusArgument &argument, const FreedesktopImageData &image);
const QDBusArgument &operator>>(const QDBusArgument &argument, FreedesktopImageData &image);

[[nodiscard]] bool decodeFreedesktopRequest(const QString &sourceService,
                                            const QString &applicationName,
                                            quint32 replacesId,
                                            const QString &applicationIcon,
                                            const QString &summary,
                                            const QString &body,
                                            const QStringList &flatActions,
                                            const QVariantMap &rawHints,
                                            int expireTimeoutMs,
                                            NotificationRequest *request,
                                            QString *error);

} // namespace QindaQt::Services::Notifications::Private

Q_DECLARE_METATYPE(QindaQt::Services::Notifications::Private::FreedesktopImageData)
