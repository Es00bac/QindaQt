// SPDX-License-Identifier: GPL-3.0-or-later
#include "result_notifier.h"

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QUrl>

namespace QindaQt::Screenshot {
namespace {

constexpr auto Service = "org.freedesktop.Notifications";
constexpr auto Path = "/org/freedesktop/Notifications";
constexpr auto Interface = "org.freedesktop.Notifications";
constexpr auto OpenKey = "open";
constexpr auto CopyKey = "copy";
constexpr auto FolderKey = "folder";
constexpr auto CopyPathKey = "copy-path";
constexpr auto SettingsKey = "settings";
// A started recording is feedback, not something to act on.
constexpr int TransientMilliseconds = 4000;
constexpr int ExpireDefault = -1;

} // namespace

ResultNotifier::ResultNotifier(QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
{
}

ResultNotifier::~ResultNotifier() = default;

QStringList ResultNotifier::actionsFor(ResultKind kind)
{
    switch (kind) {
    case ResultKind::ScreenshotSaved:
        return {QString::fromLatin1(OpenKey), tr("Open"), QString::fromLatin1(CopyKey), tr("Copy"),
                QString::fromLatin1(FolderKey), tr("Show in folder")};
    case ResultKind::RecordingSaved:
        return {QString::fromLatin1(OpenKey), tr("Open"), QString::fromLatin1(FolderKey),
                tr("Show in folder"), QString::fromLatin1(CopyPathKey), tr("Copy path")};
    case ResultKind::RecordUnavailable:
        return {QString::fromLatin1(SettingsKey), tr("Open Streaming settings")};
    case ResultKind::ScreenshotCopied:
    case ResultKind::RecordingStarted:
    case ResultKind::Failure:
        break;
    }
    return {};
}

QString ResultNotifier::summaryFor(ResultKind kind)
{
    switch (kind) {
    case ResultKind::ScreenshotSaved:
        return tr("Screenshot saved");
    case ResultKind::ScreenshotCopied:
        return tr("Screenshot copied");
    case ResultKind::RecordingSaved:
        return tr("Recording saved");
    case ResultKind::RecordingStarted:
        return tr("Recording started");
    case ResultKind::RecordUnavailable:
        return tr("OBS cannot record");
    case ResultKind::Failure:
        return tr("Screenshot");
    }
    return {};
}

QString ResultNotifier::bodyFor(ResultKind kind, const QString &path, const QString &detail)
{
    switch (kind) {
    case ResultKind::ScreenshotSaved:
    case ResultKind::RecordingSaved:
        return path.isEmpty() ? tr("OBS did not say where it saved the file.")
                              : QFileInfo(path).fileName();
    case ResultKind::ScreenshotCopied:
        return tr("The screenshot is on the clipboard.");
    case ResultKind::RecordingStarted:
        return tr("OBS is recording. Use the record shortcut again to stop.");
    case ResultKind::RecordUnavailable:
    case ResultKind::Failure:
        return detail;
    }
    return {};
}

bool ResultNotifier::start()
{
    if (m_started)
        return true;
    if (!m_connection.isConnected())
        return false;
    // AGENT-GUARD: no sender filter. The notification host may not own the
    // name yet, and a sender-filtered match registered early never fires.
    const bool actions = m_connection.connect(
        QString(), QString::fromLatin1(Path), QString::fromLatin1(Interface),
        QStringLiteral("ActionInvoked"), this, SLOT(handleActionInvoked(uint, QString)));
    const bool closed = m_connection.connect(
        QString(), QString::fromLatin1(Path), QString::fromLatin1(Interface),
        QStringLiteral("NotificationClosed"), this, SLOT(handleNotificationClosed(uint, uint)));
    m_started = actions && closed;
    return m_started;
}

void ResultNotifier::notify(ResultKind kind, const QString &path, const QString &detail)
{
    if (!m_connection.isConnected()) {
        Q_EMIT notificationFailed(QStringLiteral("no session bus for notifications"));
        return;
    }
    const QStringList actions = m_started ? actionsFor(kind) : QStringList();
    QVariantMap hints{{QStringLiteral("desktop-entry"), QStringLiteral("org.qindaqt.Screenshot")}};
    if (kind == ResultKind::ScreenshotSaved && !path.isEmpty())
        hints.insert(QStringLiteral("image-path"), QUrl::fromLocalFile(path).toString());
    if (kind == ResultKind::Failure || kind == ResultKind::RecordUnavailable)
        hints.insert(QStringLiteral("urgency"), QVariant::fromValue(uchar(1)));
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(Service), QString::fromLatin1(Path), QString::fromLatin1(Interface),
        QStringLiteral("Notify"));
    message.setArguments({
        tr("Screenshot"),
        uint(0),
        QStringLiteral("org.qindaqt.Screenshot"),
        summaryFor(kind),
        bodyFor(kind, path, detail),
        actions,
        hints,
        actions.isEmpty() ? TransientMilliseconds : ExpireDefault,
    });
    const bool trackable = !actions.isEmpty();
    // AGENT-GUARD: every post counts as live until the host answers, even
    // one without buttons; a windowless launch that exits first can drop the
    // queued Notify call and the user never hears the result.
    ++m_pending;
    Q_EMIT liveNotificationsChanged();
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, path, trackable] {
        const QDBusPendingReply<uint> reply = *watcher;
        watcher->deleteLater();
        --m_pending;
        if (reply.isError()) {
            Q_EMIT notificationFailed(reply.error().message());
        } else if (trackable) {
            const uint id = reply.value();
            m_live.insert(id, path);
            // The host may never report closing a persistent notification;
            // residency must still end.
            QTimer::singleShot(kMaxNotificationLifetimeMs, this, [this, id] { forget(id); });
        }
        Q_EMIT liveNotificationsChanged();
    });
}

void ResultNotifier::handleActionInvoked(uint id, const QString &actionKey)
{
    const auto it = m_live.constFind(id);
    if (it == m_live.constEnd())
        return;
    const QString path = it.value();
    if (actionKey == QLatin1String(OpenKey))
        Q_EMIT openRequested(path);
    else if (actionKey == QLatin1String(CopyKey))
        Q_EMIT copyImageRequested(path);
    else if (actionKey == QLatin1String(CopyPathKey))
        Q_EMIT copyPathRequested(path);
    else if (actionKey == QLatin1String(FolderKey))
        Q_EMIT showInFolderRequested(path);
    else if (actionKey == QLatin1String(SettingsKey))
        Q_EMIT settingsRequested();
}

void ResultNotifier::handleNotificationClosed(uint id, uint)
{
    forget(id);
}

void ResultNotifier::forget(uint id)
{
    if (m_live.remove(id) > 0)
        Q_EMIT liveNotificationsChanged();
}

} // namespace QindaQt::Screenshot
