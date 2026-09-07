// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/freedesktop_feedback_notifier.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QtGlobal>

namespace QindaQt::Session::DesktopControls {
namespace {

constexpr int FeedbackExpireMilliseconds = 1'200;
constexpr auto kServiceName = "org.freedesktop.Notifications";
constexpr auto kObjectPath = "/org/freedesktop/Notifications";
constexpr auto kInterfaceName = "org.freedesktop.Notifications";

QString volumeBody(int percent, bool muted)
{
    if (muted) {
        return QObject::tr("Muted");
    }
    return QString::number(percent) + QStringLiteral("%");
}

} // namespace

FreedesktopFeedbackNotifier::FreedesktopFeedbackNotifier(
    QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
{
}

FreedesktopFeedbackNotifier::~FreedesktopFeedbackNotifier() = default;

void FreedesktopFeedbackNotifier::showVolume(int percent, bool muted)
{
    notify(QStringLiteral("volume"), volumeIcon(percent, muted),
           QStringLiteral("Volume"), volumeBody(percent, muted));
}

void FreedesktopFeedbackNotifier::showBrightness(int percent)
{
    notify(QStringLiteral("brightness"), QStringLiteral("video-display"),
           QStringLiteral("Brightness"),
           QString::number(qBound(0, percent, 100)) + QStringLiteral("%"));
}

void FreedesktopFeedbackNotifier::showNotice(const QString &summary,
                                             const QString &body,
                                             const QString &iconName)
{
    notify(QStringLiteral("notice"), iconName, summary, body);
}

void FreedesktopFeedbackNotifier::notify(const QString &category,
                                         const QString &iconName,
                                         const QString &summary,
                                         const QString &body)
{
    quint32 &replacesId = category == QLatin1String("volume")
        ? m_volumeNotificationId
        : (category == QLatin1String("brightness") ? m_brightnessNotificationId
                                                   : m_noticeNotificationId);

    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("Notify"));
    message.setArguments({
        QStringLiteral("QindaQt"),
        replacesId,
        iconName,
        summary,
        body,
        QStringList{},
        QVariantMap{},
        FeedbackExpireMilliseconds,
    });
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, category, watcher] {
                const QDBusPendingReply<quint32> reply = *watcher;
                watcher->deleteLater();
                quint32 &notificationId =
                    category == QLatin1String("volume")
                        ? m_volumeNotificationId
                        : (category == QLatin1String("brightness")
                               ? m_brightnessNotificationId
                               : m_noticeNotificationId);
                if (!reply.isError()) {
                    notificationId = reply.value();
                    return;
                }
                // A lost or absent notification host must not resurrect a
                // stale id against a future server instance.
                notificationId = 0;
                Q_EMIT notificationFailed(
                    QStringLiteral("feedback-%1-failed: %2")
                        .arg(category, reply.error().message()));
            });
}

QString FreedesktopFeedbackNotifier::volumeIcon(int percent, bool muted)
{
    if (muted || percent == 0) {
        return QStringLiteral("audio-volume-muted");
    }
    if (percent <= 33) {
        return QStringLiteral("audio-volume-low");
    }
    if (percent <= 66) {
        return QStringLiteral("audio-volume-medium");
    }
    return QStringLiteral("audio-volume-high");
}

} // namespace QindaQt::Session::DesktopControls
