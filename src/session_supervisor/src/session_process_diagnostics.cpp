// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/session_supervisor/session_process_supervisor.h>
#include <qindaqt/session_supervisor/session_optional_child.h>
#include "first_launch_welcome.h"
namespace QindaQt::SessionSupervisor {
bool SessionProcessSupervisor::isRunning() const noexcept
{
    // A missing shell is a recoverable interval. The notification host keeps
    // the session's authenticated service resident while the retry timer
    // paces a replacement.
    return m_running && m_host.state() != QProcess::NotRunning;
}

bool SessionProcessSupervisor::canLogout() const noexcept
{
    // Session1 authorizes the shell by PID. During a replacement interval
    // there is no shell caller to authorize, even though the session remains
    // alive and the resident host continues serving applications.
    return isRunning() && !m_stopping
           && m_shell.state() != QProcess::NotRunning;
}

qint64 SessionProcessSupervisor::notificationHostProcessId() const noexcept
{
    return m_host.state() == QProcess::NotRunning ? 0 : m_hostProcessId;
}

qint64 SessionProcessSupervisor::shellProcessId() const noexcept
{
    return m_shell.state() == QProcess::NotRunning ? 0 : m_shellProcessId;
}

int SessionProcessSupervisor::shellRestartCount() const noexcept { return m_shellRestartCount; }

qint64 SessionProcessSupervisor::networkSecretAgentProcessId() const noexcept
{
    return m_networkSecretAgent.state() == QProcess::NotRunning
        ? 0 : m_networkSecretAgentProcessId;
}

int SessionProcessSupervisor::networkSecretAgentRestartCount() const noexcept
{
    return m_networkSecretAgentRestartCount;
}

qint64 SessionProcessSupervisor::desktopControlsProcessId() const noexcept
{
    return m_desktopControls->processId();
}

int SessionProcessSupervisor::desktopControlsRestartCount() const noexcept
{
    return m_desktopControls->restartCount();
}

qint64 SessionProcessSupervisor::powerDevilProcessId() const noexcept
{
    return m_powerDevil->processId();
}

qint64 SessionProcessSupervisor::globalShortcutDaemonProcessId() const noexcept
{
    return m_globalShortcutDaemon->processId();
}

qint64 SessionProcessSupervisor::inputMethodDaemonProcessId() const noexcept
{
    return m_inputMethodDaemon->processId();
}


qint64 SessionProcessSupervisor::xembedTrayProxyProcessId() const noexcept
{
    return m_xembedTrayProxy->processId();
}

qint64 SessionProcessSupervisor::polkitAgentProcessId() const noexcept
{
    return m_polkitAgent->processId();
}

int SessionProcessSupervisor::polkitAgentRestartCount() const noexcept
{
    return m_polkitAgent->restartCount();
}

qint64 SessionProcessSupervisor::welcomeProcessId() const noexcept
{
    return m_welcome->processId();
}

}
