// SPDX-License-Identifier: LGPL-3.0-or-later

#include "session_wayland_display_p.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusVariant>

#include <sys/stat.h>

#include <utility>

namespace QindaQt::DisplayRuntime
{

QString waylandDisplayFromEnvironment(const QStringList &assignments)
{
    static const QString prefix = QStringLiteral("WAYLAND_DISPLAY=");
    QString value;
    for (const QString &assignment : assignments) {
        if (assignment.startsWith(prefix)) {
            value = assignment.mid(prefix.size()); // the last assignment wins
        }
    }
    return value;
}

bool waylandSocketPresent(const QString &runtimeDirectory, const QString &display)
{
    if (display.isEmpty() || display.contains(QChar(u'\0'))) {
        return false;
    }
    QString path = display;
    if (!QDir::isAbsolutePath(display)) {
        if (runtimeDirectory.isEmpty() || display.contains(QLatin1Char('/'))) {
            return false;
        }
        path = runtimeDirectory + QLatin1Char('/') + display;
    }
    struct stat status {};
    return ::stat(QFile::encodeName(path).constData(), &status) == 0
        && S_ISSOCK(status.st_mode);
}

WaylandDisplayWait::WaylandDisplayWait(QDBusConnection sessionBus,
                                       QString processValue,
                                       QString runtimeDirectory,
                                       WaylandDisplayWaitOptions options,
                                       QObject *parent)
    : QObject(parent)
    , m_bus(std::move(sessionBus))
    , m_processValue(std::move(processValue))
    , m_runtimeDirectory(std::move(runtimeDirectory))
    , m_options(std::move(options))
    , m_backoff(m_options.policy, m_options.timers)
{
    connect(&m_backoff, &Platform::Recovery::RecoveryBackoff::retry, this,
            &WaylandDisplayWait::attempt);
}

void WaylandDisplayWait::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    attempt();
}

void WaylandDisplayWait::attempt()
{
    if (m_finished) {
        return;
    }
    ++m_attempts;
    if (waylandSocketPresent(m_runtimeDirectory, m_processValue)) {
        conclude(m_processValue, {});
        return;
    }
    if (!m_bus.isConnected()) {
        retry(QStringLiteral("session bus is disconnected"));
        return;
    }
    // The session supervisor publishes WAYLAND_DISPLAY to the systemd user
    // manager (SetEnvironment) right after the compositor hands it the socket.
    QDBusMessage query = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.systemd1"),
        QStringLiteral("/org/freedesktop/systemd1"),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    query << QStringLiteral("org.freedesktop.systemd1.Manager")
          << QStringLiteral("Environment");
    auto *watcher = new QDBusPendingCallWatcher(
        m_bus.asyncCall(query, m_options.callTimeoutMilliseconds), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (m_finished) {
            return;
        }
        QString published;
        if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1) {
            const QVariant value =
                qvariant_cast<QDBusVariant>(reply.arguments().constFirst()).variant();
            published = waylandDisplayFromEnvironment(value.toStringList());
        }
        if (waylandSocketPresent(m_runtimeDirectory, published)) {
            conclude(published, {});
            return;
        }
        retry(published.isEmpty()
                  ? QStringLiteral("the session has not published WAYLAND_DISPLAY")
                  : QStringLiteral("published display %1 has no socket").arg(published));
    });
}

void WaylandDisplayWait::retry(const QString &reason)
{
    if (m_attempts >= m_options.maximumAttempts) {
        conclude({}, reason);
        return;
    }
    m_backoff.schedule();
}

void WaylandDisplayWait::conclude(const QString &display, const QString &failure)
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_backoff.cancel();
    m_display = display;
    m_failure = failure;
    Q_EMIT done();
}

} // namespace QindaQt::DisplayRuntime
