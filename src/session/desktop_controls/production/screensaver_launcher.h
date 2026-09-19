// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/session/desktop_controls/screensaver_preferences.h>

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace QindaQt::Session::DesktopControls {

// Idle screensaver: after the configured idle time, start the chosen saver
// program on every output (qinda-patrol --screensaver, circuit-reef
// --all-screens); stop it on user activity or when KScreenLocker locks the
// session. The locker (KWin's embedded KScreenLocker) stays the only lock
// authority; the saver is decoration only and is never started while locked,
// so it never sits between the user and the password prompt.
//
// Configuration is the purpose-scoped Settings1 pair `power.screensaver` and
// `power.screensaverMinutes`, edited in Settings -> Power. Changing either one
// re-arms immediately; switching saver while one is running replaces it.
class ScreensaverLauncher final : public QObject {
    Q_OBJECT

public:
    ScreensaverLauncher(QDBusConnection sessionBus,
                        ScreensaverPreferencesProvider &preferences,
                        QObject *parent = nullptr);
    ~ScreensaverLauncher() override;

    ScreensaverLauncher(const ScreensaverLauncher &) = delete;
    ScreensaverLauncher &operator=(const ScreensaverLauncher &) = delete;

    void start();

private Q_SLOTS:
    void lockChanged(bool active);

private:
    void applyPreferences(const ScreensaverPreferences &preferences);
    void armIdleTimeout();
    void disarmIdleTimeout();
    void launch();
    void stop();
    [[nodiscard]] bool sessionLocked() const;

    QDBusConnection m_bus;
    ScreensaverPreferencesProvider &m_preferences;
    ScreensaverPreferences m_current;
    QProcess m_process;
    QTimer m_killTimer;
    QTimer m_relaunchTimer;
    // Guards against a saver that starts and exits immediately (a missing
    // platform plugin, say) turning the relaunch path into a spawn loop.
    QElapsedTimer m_runtime;
    int m_quickExits = 0;
    int m_idleToken = -1;
    bool m_idle = false;
    bool m_locked = false;
};

} // namespace QindaQt::Session::DesktopControls
