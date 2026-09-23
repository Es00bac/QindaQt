// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/session_autostart/autostart_catalog.h>

#include <QProcess>
#include <QString>

#include <memory>
#include <vector>

namespace QindaQt::SessionSupervisor {

// One supervisor-owned login batch. Only eligible direct children started by
// this object are retained or stopped; short-lived commands may exit normally
// and are never restarted. Calling startOnce again during a shell replacement
// does nothing. stop() resets the guard for a new supervised login.
class SessionAutostartRunner final {
public:
    explicit SessionAutostartRunner(SessionAutostart::ScanOptions options);
    ~SessionAutostartRunner();

    SessionAutostartRunner(const SessionAutostartRunner &) = delete;
    SessionAutostartRunner &operator=(const SessionAutostartRunner &) = delete;

    void startOnce();
    void stop() noexcept;
    [[nodiscard]] bool started() const noexcept { return m_started; }
    [[nodiscard]] int launchedCount() const noexcept { return m_launchedCount; }

private:
    SessionAutostart::ScanOptions m_options;
    std::vector<std::unique_ptr<QProcess>> m_processes;
    bool m_started = false;
    int m_launchedCount = 0;
};

} // namespace QindaQt::SessionSupervisor
