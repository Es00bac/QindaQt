// SPDX-License-Identifier: LGPL-3.0-or-later

#include "freedesktop_notification_codec_p.h"

#include "qindaqt/services/notifications/notification_limits.h"

#include <QDBusMetaType>
#include <QDBusVariant>

namespace QindaQt::Services::Notifications::Private {
namespace {

constexpr qsizetype MaximumHintCount = 64;

QVariant unwrapped(QVariant value)
{
    while (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
        value = value.value<QDBusVariant>().variant();
    }
    return value;
}

bool optionalString(const QVariantMap &hints,
                    const QStringList &keys,
                    QString *target,
                    QString *error)
{
    for (const auto &key : keys) {
        const auto iterator = hints.constFind(key);
        if (iterator == hints.cend()) {
            continue;
        }
        const QVariant value = unwrapped(*iterator);
        if (value.metaType() != QMetaType::fromType<QString>()) {
            *error = QStringLiteral("hint '%1' must be a string").arg(key);
            return false;
        }
        *target = value.toString();
        return true;
    }
    return true;
}

bool optionalBool(const QVariantMap &hints,
                  const QStringList &keys,
                  bool *target,
                  QString *error)
{
    for (const auto &key : keys) {
        const auto iterator = hints.constFind(key);
        if (iterator == hints.cend()) {
            continue;
        }
        const QVariant value = unwrapped(*iterator);
        if (value.metaType() != QMetaType::fromType<bool>()) {
            *error = QStringLiteral("hint '%1' must be a boolean").arg(key);
            return false;
        }
        *target = value.toBool();
        return true;
    }
    return true;
}

bool decodeUrgency(const QVariantMap &hints, Urgency *target, QString *error)
{
    const auto iterator = hints.constFind(QStringLiteral("urgency"));
    if (iterator == hints.cend()) {
        return true;
    }
    const QVariant value = unwrapped(*iterator);
    if (value.metaType() != QMetaType::fromType<uchar>()) {
        *error = QStringLiteral("hint 'urgency' must be a byte from 0 through 2");
        return false;
    }
    const uint raw = value.value<uchar>();
    const auto urgency = static_cast<Urgency>(raw);
    if (!isValidUrgency(urgency)) {
        *error = QStringLiteral("hint 'urgency' must be a byte from 0 through 2");
        return false;
    }
    *target = urgency;
    return true;
}

bool decodeImage(const QVariantMap &hints,
                 std::optional<NotificationImage> *target,
                 QString *error)
{
    static const QMetaType registration = qDBusRegisterMetaType<FreedesktopImageData>();
    Q_UNUSED(registration)

    const QStringList keys = {
        QStringLiteral("image-data"),
        QStringLiteral("image_data"),
        QStringLiteral("icon_data"),
    };
    for (const auto &key : keys) {
        const auto iterator = hints.constFind(key);
        if (iterator == hints.cend()) {
            continue;
        }
        const QVariant value = unwrapped(*iterator);
        if (value.metaType() != QMetaType::fromType<QDBusArgument>()
            && !value.canConvert<FreedesktopImageData>()) {
            *error = QStringLiteral("hint '%1' must use image-data struct encoding").arg(key);
            return false;
        }
        const FreedesktopImageData raw = value.metaType() == QMetaType::fromType<QDBusArgument>()
            ? qdbus_cast<FreedesktopImageData>(value.value<QDBusArgument>())
            : value.value<FreedesktopImageData>();
        *target = NotificationImage{
            .width = raw.width,
            .height = raw.height,
            .rowStride = raw.rowStride,
            .hasAlpha = raw.hasAlpha,
            .bitsPerSample = raw.bitsPerSample,
            .channels = raw.channels,
            .pixels = raw.pixels,
        };
        return true;
    }
    return true;
}

} // namespace

QDBusArgument &operator<<(QDBusArgument &argument, const FreedesktopImageData &image)
{
    argument.beginStructure();
    argument << image.width << image.height << image.rowStride << image.hasAlpha
             << image.bitsPerSample << image.channels << image.pixels;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, FreedesktopImageData &image)
{
    argument.beginStructure();
    argument >> image.width >> image.height >> image.rowStride >> image.hasAlpha
        >> image.bitsPerSample >> image.channels >> image.pixels;
    argument.endStructure();
    return argument;
}

bool decodeFreedesktopRequest(const QString &sourceService,
                              const QString &applicationName,
                              quint32 replacesId,
                              const QString &applicationIcon,
                              const QString &summary,
                              const QString &body,
                              const QStringList &flatActions,
                              const QVariantMap &rawHints,
                              int expireTimeoutMs,
                              NotificationRequest *request,
                              QString *error)
{
    if (request == nullptr || error == nullptr) {
        return false;
    }
    if (flatActions.size() % 2 != 0
        || flatActions.size() / 2 > NotificationLimits::MaximumActionCount) {
        *error = QStringLiteral("actions must contain bounded key/label pairs");
        return false;
    }
    if (rawHints.size() > MaximumHintCount) {
        *error = QStringLiteral("notification contains too many hints");
        return false;
    }

    NotificationRequest candidate;
    candidate.sourceService = sourceService;
    candidate.applicationName = applicationName;
    candidate.replacesId = replacesId;
    candidate.applicationIcon = applicationIcon;
    candidate.summary = summary;
    candidate.body = body;
    candidate.expireTimeoutMs = expireTimeoutMs;
    candidate.actions.reserve(flatActions.size() / 2);
    for (qsizetype index = 0; index < flatActions.size(); index += 2) {
        candidate.actions.push_back(NotificationAction{
            flatActions.at(index),
            flatActions.at(index + 1),
        });
    }

    auto &hints = candidate.hints;
    if (!decodeUrgency(rawHints, &hints.urgency, error)
        || !optionalString(rawHints,
                           {QStringLiteral("category")},
                           &hints.category,
                           error)
        || !optionalString(rawHints,
                           {QStringLiteral("desktop-entry"),
                            QStringLiteral("desktop_entry")},
                           &hints.desktopEntry,
                           error)
        || !optionalString(rawHints,
                           {QStringLiteral("image-path"),
                            QStringLiteral("image_path")},
                           &hints.imagePath,
                           error)
        || !optionalString(rawHints,
                           {QStringLiteral("sound-file"),
                            QStringLiteral("sound_file")},
                           &hints.soundFile,
                           error)
        || !optionalString(rawHints,
                           {QStringLiteral("sound-name"),
                            QStringLiteral("sound_name")},
                           &hints.soundName,
                           error)
        || !optionalBool(rawHints,
                         {QStringLiteral("action-icons"),
                          QStringLiteral("action_icons")},
                         &hints.actionIcons,
                         error)
        || !optionalBool(rawHints,
                         {QStringLiteral("resident")},
                         &hints.resident,
                         error)
        || !optionalBool(rawHints,
                         {QStringLiteral("transient")},
                         &hints.transient,
                         error)
        || !optionalBool(rawHints,
                         {QStringLiteral("suppress-sound"),
                          QStringLiteral("suppress_sound")},
                         &hints.suppressSound,
                         error)
        || !decodeImage(rawHints, &hints.image, error)) {
        return false;
    }

    *request = std::move(candidate);
    return true;
}

} // namespace QindaQt::Services::Notifications::Private
