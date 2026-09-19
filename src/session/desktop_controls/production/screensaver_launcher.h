// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

namespace QindaQt::Session::DesktopControls {

// Idle screensaver: after the configured idle time, start the chosen saver
// program on every output (qinda-patrol --screensaver, circuit-reef
// --all-screens); stop it on user activity or when KScreenLocker locks the
// session. The locker (KWin's embedded KScreenLocker) stays the only lock
// authority; the saver is decoration only and is never started while locked.
//
// Configuration: ~/.config/qindaqt/screensaver.json
//   {"enabled": true, "saver": "qinda-patrol", "timeoutMinutes": 5,
//    "arguments": ["--screensaver", "--no-metrics"]}
// "saver" is a program name on PATH or an absolute path; "arguments" is
// optional and defaults per known saver. The file is watched and reloaded.
class ScreensaverLauncher final : public QObject {
    Q_OBJECT

public:
    explicit ScreensaverLauncher(QDBusConnection sessionBus, QObject *parent = nullptr);
    ~ScreensaverLauncher() override;

    void start();

private Q_SLOTS:
    void lockChanged(bool active);

private:
    void loadConfiguration();
    void armIdleTimeout();
    void disarmIdleTimeout();
    void launch();
    void stop();
    bool sessionLocked() const;

    QDBusConnection m_bus;
    QString m_configPath;
    QFileSystemWatcher m_watcher;
    QProcess m_process;
    QTimer m_killTimer;
    QTimer m_relaunchTimer;
    bool m_enabled = true;
    QString m_program = QStringLiteral("qinda-patrol");
    QStringList m_arguments;
    int m_timeoutMinutes = 5;
    int m_idleToken = -1;
    bool m_idle = false;
    bool m_locked = false;
};

} // namespace QindaQt::Session::DesktopControls
