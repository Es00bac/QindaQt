// SPDX-License-Identifier: LGPL-3.0-or-later

#include "notification_validation_p.h"

#include "qindaqt/services/notifications/notification_limits.h"

#include <QSet>

#include <limits>

namespace QindaQt::Services::Notifications::Private {
namespace {

bool isWellFormedUtf16(const QString &value)
{
    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar current = value.at(index);
        if (current.isHighSurrogate()) {
            if (index + 1 >= value.size() || !value.at(index + 1).isLowSurrogate()) {
                return false;
            }
            ++index;
        } else if (current.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

bool validateText(const QString &value,
                  qsizetype maximumBytes,
                  const QString &field,
                  bool requireNonEmpty,
                  QString *error)
{
    if (requireNonEmpty && value.isEmpty()) {
        *error = QStringLiteral("%1 must not be empty").arg(field);
        return false;
    }
    // One UTF-16 code unit always produces at least one UTF-8 byte. Checking
    // this first bounds the allocation performed by toUtf8().
    if (value.size() > maximumBytes || value.contains(QChar::Null)
        || !isWellFormedUtf16(value) || value.toUtf8().size() > maximumBytes) {
        *error = QStringLiteral("%1 is not valid bounded text").arg(field);
        return false;
    }
    return true;
}

bool validateImage(const NotificationImage &image, QString *error)
{
    if (image.width <= 0 || image.height <= 0
        || image.width > NotificationLimits::MaximumImageDimension
        || image.height > NotificationLimits::MaximumImageDimension) {
        *error = QStringLiteral("image dimensions are outside the supported range");
        return false;
    }
    if (image.bitsPerSample != 8
        || image.channels != (image.hasAlpha ? 4 : 3)) {
        *error = QStringLiteral("image must contain 8-bit RGB or RGBA pixels");
        return false;
    }

    const qint64 minimumStride = qint64(image.width) * qint64(image.channels);
    if (image.rowStride < minimumStride) {
        *error = QStringLiteral("image row stride is smaller than one pixel row");
        return false;
    }
    const qint64 expectedBytes = qint64(image.rowStride) * qint64(image.height);
    if (expectedBytes > NotificationLimits::MaximumImageBytes
        || expectedBytes != image.pixels.size()) {
        *error = QStringLiteral("image pixel payload does not match its bounded geometry");
        return false;
    }
    return true;
}

} // namespace

bool validateSourceService(const QString &sourceService, QString *error)
{
    return validateText(sourceService,
                        NotificationLimits::MaximumSourceServiceBytes,
                        QStringLiteral("sourceService"),
                        true,
                        error);
}

bool validateActionInvocation(const QString &actionKey,
                              const QString &activationToken,
                              QString *error)
{
    return validateText(actionKey,
                        NotificationLimits::MaximumActionKeyBytes,
                        QStringLiteral("actionKey"),
                        true,
                        error)
        && validateText(activationToken,
                        NotificationLimits::MaximumActivationTokenBytes,
                        QStringLiteral("activationToken"),
                        false,
                        error);
}

bool validateRequest(const NotificationRequest &request, QString *error)
{
    if (!validateSourceService(request.sourceService, error)
        || !validateText(request.applicationName,
                         NotificationLimits::MaximumApplicationNameBytes,
                         QStringLiteral("applicationName"),
                         false,
                         error)
        || !validateText(request.applicationIcon,
                         NotificationLimits::MaximumIconBytes,
                         QStringLiteral("applicationIcon"),
                         false,
                         error)
        || !validateText(request.summary,
                         NotificationLimits::MaximumSummaryBytes,
                         QStringLiteral("summary"),
                         false,
                         error)
        || !validateText(request.body,
                         NotificationLimits::MaximumBodyBytes,
                         QStringLiteral("body"),
                         false,
                         error)) {
        return false;
    }
    if (request.expireTimeoutMs < -1) {
        *error = QStringLiteral("expireTimeoutMs must be -1, 0, or positive");
        return false;
    }
    if (request.actions.size() > NotificationLimits::MaximumActionCount) {
        *error = QStringLiteral("too many notification actions");
        return false;
    }

    QSet<QString> actionKeys;
    for (const auto &action : request.actions) {
        if (!validateText(action.key,
                          NotificationLimits::MaximumActionKeyBytes,
                          QStringLiteral("action key"),
                          true,
                          error)
            || !validateText(action.label,
                             NotificationLimits::MaximumActionLabelBytes,
                             QStringLiteral("action label"),
                             false,
                             error)) {
            return false;
        }
        if (actionKeys.contains(action.key)) {
            *error = QStringLiteral("action keys must be unique");
            return false;
        }
        actionKeys.insert(action.key);
    }

    const auto &hints = request.hints;
    if (!isValidUrgency(hints.urgency)) {
        *error = QStringLiteral("urgency is outside the supported range");
        return false;
    }
    if (!validateText(hints.category,
                      NotificationLimits::MaximumMetadataTextBytes,
                      QStringLiteral("category"),
                      false,
                      error)
        || !validateText(hints.desktopEntry,
                         NotificationLimits::MaximumMetadataTextBytes,
                         QStringLiteral("desktopEntry"),
                         false,
                         error)
        || !validateText(hints.imagePath,
                         NotificationLimits::MaximumIconBytes,
                         QStringLiteral("imagePath"),
                         false,
                         error)
        || !validateText(hints.soundFile,
                         NotificationLimits::MaximumIconBytes,
                         QStringLiteral("soundFile"),
                         false,
                         error)
        || !validateText(hints.soundName,
                         NotificationLimits::MaximumMetadataTextBytes,
                         QStringLiteral("soundName"),
                         false,
                         error)) {
        return false;
    }
    if (hints.image.has_value() && !validateImage(*hints.image, error)) {
        return false;
    }
    return true;
}

qsizetype retainedPayloadBytes(const NotificationRequest &request)
{
    qsizetype bytes = request.sourceService.toUtf8().size()
        + request.applicationName.toUtf8().size()
        + request.applicationIcon.toUtf8().size()
        + request.summary.toUtf8().size()
        + request.body.toUtf8().size();
    for (const auto &action : request.actions) {
        bytes += action.key.toUtf8().size() + action.label.toUtf8().size();
    }
    const auto &hints = request.hints;
    bytes += hints.category.toUtf8().size() + hints.desktopEntry.toUtf8().size()
        + hints.imagePath.toUtf8().size() + hints.soundFile.toUtf8().size()
        + hints.soundName.toUtf8().size();
    if (hints.image.has_value()) {
        bytes += hints.image->pixels.size();
    }
    return bytes;
}

} // namespace QindaQt::Services::Notifications::Private
