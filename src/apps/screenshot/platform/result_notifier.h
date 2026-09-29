// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace QindaQt::Screenshot {

// What a notification is about; it decides the text and the actions.
enum class ResultKind {
    ScreenshotSaved,
    ScreenshotCopied,
    RecordingSaved,
    RecordingStarted,
    // OBS cannot be driven; the one action opens Settings → Streaming.
    RecordUnavailable,
    Failure,
};

// Posts results to the desktop's notification service
// (org.freedesktop.Notifications, served by QindaQt's notification host)
// and routes its action buttons back.
//
// AGENT-CONTRACT: action keys `open`, `copy`, `folder`, `copy-path` and
// `settings` are
// the exact strings ActionInvoked carries back. The notifier ignores
// actions for notifications it did not post, because every process on the
// bus sees every ActionInvoked. `hasLiveNotifications()` is what keeps a
// windowless launch alive long enough to answer a button; it is bounded by
// `kMaxNotificationLifetimeMs` even when the host never reports closing.
class ResultNotifier final : public QObject {
    Q_OBJECT

public:
    static constexpr int kMaxNotificationLifetimeMs = 10 * 60 * 1000;

    explicit ResultNotifier(QDBusConnection connection, QObject *parent = nullptr);
    ~ResultNotifier() override;

    // Subscribes to ActionInvoked/NotificationClosed. Returns false when the
    // bus is unavailable; notify() then reports failure instead of posting.
    bool start();
    void notify(ResultKind kind, const QString &path, const QString &detail = {});

    [[nodiscard]] bool hasLiveNotifications() const { return !m_live.isEmpty() || m_pending > 0; }

    // Pure presentation, exported so rows can pin what a user reads.
    [[nodiscard]] static QStringList actionsFor(ResultKind kind);
    [[nodiscard]] static QString summaryFor(ResultKind kind);
    [[nodiscard]] static QString bodyFor(ResultKind kind, const QString &path, const QString &detail);

Q_SIGNALS:
    void openRequested(const QString &path);
    void copyImageRequested(const QString &path);
    void copyPathRequested(const QString &path);
    void showInFolderRequested(const QString &path);
    void settingsRequested();
    void liveNotificationsChanged();
    void notificationFailed(const QString &message);

private Q_SLOTS:
    void handleActionInvoked(uint id, const QString &actionKey);
    void handleNotificationClosed(uint id, uint reason);

private:
    void forget(uint id);

    QDBusConnection m_connection;
    QHash<uint, QString> m_live;
    // Posted but not yet answered with an id; counts as live for residency.
    int m_pending = 0;
    bool m_started = false;
};

} // namespace QindaQt::Screenshot
