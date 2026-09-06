// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QProcess>
#include <QString>

namespace QindaQt::SessionSupervisor {

// Owns the optional Welcome child for one session. The child is parent-death
// bound and explicitly stopped, but an exit never restarts it or affects the
// essential shell/notification-host lifetime.
class FirstLaunchWelcome final {
public:
    FirstLaunchWelcome();
    ~FirstLaunchWelcome();

    void start(const QString &program);
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] qint64 processId() const noexcept;

private:
    QProcess m_process;
    qint64 m_processId = 0;
};

} // namespace QindaQt::SessionSupervisor
