// SPDX-License-Identifier: LGPL-3.0-or-later
#include "screensaver_launcher.h"

#include <qindaqt/session/desktop_controls/screensaver_catalog.h>

#include <KIdleTime>

#include <QDBusInterface>
#include <QDBusReply>
#include <QTextStream>

namespace QindaQt::Session::DesktopControls {
namespace {

constexpr auto kScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto kScreenSaverPath = "/ScreenSaver";
constexpr int millisecondsPerMinute = 60'000;
// A saver that dies sooner than this did not really run.
constexpr qint64 shortRunMilliseconds = 5'000;
constexpr int maximumQuickExits = 3;

} // namespace

ScreensaverLauncher::ScreensaverLauncher(QDBusConnection sessionBus,
                                         ScreensaverPreferencesProvider &preferences,
                                         const ScreensaverCatalog &catalog,
                                         QObject *parent)
    : QObject(parent), m_bus(std::move(sessionBus)), m_preferences(preferences),
      m_catalog(catalog)
{
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(2000);
    m_relaunchTimer.setSingleShot(true);
    m_relaunchTimer.setInterval(1000);

    connect(&m_killTimer, &QTimer::timeout, this, [this] {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
        }
    });
    connect(&m_relaunchTimer, &QTimer::timeout, this, [this] {
        // The saver exits on an output topology change; bring it back while
        // the session is still idle and unlocked.
        if (m_idle && !m_locked && m_current.enabled()) {
            launch();
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        m_killTimer.stop();
        if (m_runtime.isValid() && m_runtime.elapsed() < shortRunMilliseconds) {
            ++m_quickExits;
        } else {
            m_quickExits = 0;
        }
        m_runtime.invalidate();
        if (!m_idle || m_locked || !m_current.enabled()) {
            return;
        }
        if (m_quickExits >= maximumQuickExits) {
            QTextStream(stderr)
                << "qindaqt-desktop-controls: screensaver exited immediately "
                << m_quickExits << " times; not restarting until the next resume: "
                << m_current.saver << '\n';
            return;
        }
        m_relaunchTimer.start();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            QTextStream(stderr) << "qindaqt-desktop-controls: screensaver failed to start: "
                                << m_current.saver << '\n';
            // No relaunch storm for a program that is simply not installed.
            m_idle = false;
            m_runtime.invalidate();
        }
    });

    auto *const idleTime = KIdleTime::instance();
    connect(idleTime, &KIdleTime::timeoutReached, this, [this](int identifier, int) {
        if (identifier != m_idleToken) {
            return;
        }
        m_idle = true;
        KIdleTime::instance()->catchNextResumeEvent();
        if (!m_locked && !sessionLocked()) {
            launch();
        }
    });
    connect(idleTime, &KIdleTime::resumingFromIdle, this, [this] {
        m_idle = false;
        m_quickExits = 0;
        m_relaunchTimer.stop();
        stop();
    });

    connect(&m_preferences, &ScreensaverPreferencesProvider::preferencesChanged, this,
            [this](ScreensaverPreferences next) { applyPreferences(next); });
}

ScreensaverLauncher::~ScreensaverLauncher()
{
    disarmIdleTimeout();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(500);
    }
}

void ScreensaverLauncher::start()
{
    m_bus.connect(QString::fromLatin1(kScreenSaverService), QString::fromLatin1(kScreenSaverPath),
                  QString::fromLatin1(kScreenSaverService), QStringLiteral("ActiveChanged"), this,
                  SLOT(lockChanged(bool)));
    applyPreferences(m_preferences.currentPreferences());
    m_preferences.refresh();
}

void ScreensaverLauncher::applyPreferences(const ScreensaverPreferences &preferences)
{
    const ScreensaverPreferences previous = m_current;
    m_current = preferences;
    if (previous == m_current) {
        return;
    }
    // A saver already on screen must not outlive the choice that started it.
    // The finished handler relaunches the new one while the session is idle.
    // The token is the program identity: the catalog resolves it at launch.
    if (previous.saver != m_current.saver) {
        m_quickExits = 0;
        stop();
    }
    armIdleTimeout();
}

void ScreensaverLauncher::armIdleTimeout()
{
    disarmIdleTimeout();
    if (!m_current.enabled()) {
        m_relaunchTimer.stop();
        stop();
        return;
    }
    m_idleToken =
        KIdleTime::instance()->addIdleTimeout(m_current.minutes * millisecondsPerMinute);
}

void ScreensaverLauncher::disarmIdleTimeout()
{
    if (m_idleToken >= 0) {
        KIdleTime::instance()->removeIdleTimeout(m_idleToken);
        m_idleToken = -1;
    }
}

bool ScreensaverLauncher::sessionLocked() const
{
    QDBusInterface locker(QString::fromLatin1(kScreenSaverService),
                          QString::fromLatin1(kScreenSaverPath),
                          QString::fromLatin1(kScreenSaverService), m_bus);
    const QDBusReply<bool> reply = locker.call(QStringLiteral("GetActive"));
    return reply.isValid() && reply.value();
}

void ScreensaverLauncher::launch()
{
    if (m_process.state() != QProcess::NotRunning || !m_current.enabled()) {
        return;
    }
    // AGENT-GUARD: the program and its arguments come from the discovered
    // catalog entry, never from the persisted token itself. A saver whose
    // package was removed between snapshot and idle simply does not start.
    const auto entry = m_catalog.entry(m_current.saver);
    if (!entry.has_value()) {
        return;
    }
    m_killTimer.stop();
    m_runtime.start();
    m_process.start(entry->token, entry->arguments);
}

void ScreensaverLauncher::stop()
{
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }
    m_process.terminate();
    m_killTimer.start();
}

// The Q_SLOT the D-Bus signal connects to: KScreenLocker's ActiveChanged.
void ScreensaverLauncher::lockChanged(bool active)
{
    m_locked = active;
    if (active) {
        m_relaunchTimer.stop();
        stop();
    }
}

} // namespace QindaQt::Session::DesktopControls
