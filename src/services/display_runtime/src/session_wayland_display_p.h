// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// Private to the packaged Display1 process and its tests; not installed.

#include <qindaqt/platform/recovery_backoff/recovery_backoff.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtDBus/QDBusConnection>

#include <chrono>

namespace QindaQt::DisplayRuntime
{

// WAYLAND_DISPLAY from a manager environment block of NAME=value entries, or
// empty when absent.
[[nodiscard]] QString waylandDisplayFromEnvironment(const QStringList &assignments);

// True when the display names an existing socket: an absolute name directly,
// otherwise inside runtimeDirectory (libwayland's own resolution rule).
[[nodiscard]] bool waylandSocketPresent(const QString &runtimeDirectory,
                                        const QString &display);

struct WaylandDisplayWaitOptions final {
    // 0.25 s doubling to 2 s over ten attempts is about 15 s: inside the
    // unit's 20 s TimeoutStartSec, so systemd never kills a waiting start.
    Platform::Recovery::BackoffPolicy policy{std::chrono::milliseconds{250},
                                             std::chrono::milliseconds{2'000}};
    Platform::Recovery::RecoveryTimerFactory timers{};
    int maximumAttempts = 10;
    int callTimeoutMilliseconds = 1'000;
};

// AGENT-CONTRACT (ADR-0379): Display1 can be activated (KWin -> NightLight ->
// Display1) before qindaqt-session publishes the session's WAYLAND_DISPLAY.
// The process value is used when its socket exists; otherwise the value the
// systemd user manager now publishes. Never a guessed or scanned socket.
// Bounded: done() fires exactly once, with a display or a failure reason.
// Same-thread; the connection outlives the wait.
class WaylandDisplayWait final : public QObject
{
    Q_OBJECT

public:
    WaylandDisplayWait(QDBusConnection sessionBus, QString processValue,
                       QString runtimeDirectory,
                       WaylandDisplayWaitOptions options = {},
                       QObject *parent = nullptr);
    void start();
    [[nodiscard]] bool finished() const noexcept { return m_finished; }
    [[nodiscard]] QString display() const { return m_display; }
    [[nodiscard]] QString failure() const { return m_failure; }
    [[nodiscard]] int attempts() const noexcept { return m_attempts; }

Q_SIGNALS:
    void done();

private:
    void attempt();
    void retry(const QString &reason);
    void conclude(const QString &display, const QString &failure);

    QDBusConnection m_bus;
    QString m_processValue;
    QString m_runtimeDirectory;
    WaylandDisplayWaitOptions m_options;
    Platform::Recovery::RecoveryBackoff m_backoff;
    QString m_display;
    QString m_failure;
    int m_attempts = 0;
    bool m_started = false;
    bool m_finished = false;
};

} // namespace QindaQt::DisplayRuntime
