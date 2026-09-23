// SPDX-License-Identifier: LGPL-3.0-or-later
#include "session_autostart_runner.h"

#include "qindaqt/session_supervisor/supervised_process_launcher.h"

#include <QLoggingCategory>

#include <utility>

namespace QindaQt::SessionSupervisor {
namespace {
constexpr int StopTimeoutMilliseconds = 2'000;
Q_LOGGING_CATEGORY(SESSION_AUTOSTART, "qindaqt.session-autostart")
}

SessionAutostartRunner::SessionAutostartRunner(SessionAutostart::ScanOptions options)
    : m_options(std::move(options)) {}

SessionAutostartRunner::~SessionAutostartRunner() { stop(); }

void SessionAutostartRunner::startOnce()
{
    if (m_started)
        return;
    m_started = true;
    m_launchedCount = 0;
    // Empty roots are the explicit private-session/no-autostart contract.
    if (m_options.userDirectory.isEmpty() && m_options.systemDirectories.isEmpty())
        return;

    QString error;
    const auto entries = SessionAutostart::scan(m_options, &error);
    if (!error.isEmpty())
        qCWarning(SESSION_AUTOSTART) << "could not scan autostart:" << error;
    for (const auto &entry : entries) {
        if (!entry.eligible)
            continue;
        auto process = std::make_unique<QProcess>();
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        if (!entry.workingDirectory.isEmpty())
            process->setWorkingDirectory(entry.workingDirectory);
        const QString id = entry.id;
        QObject::connect(process.get(), &QProcess::errorOccurred, process.get(),
                         [id](QProcess::ProcessError processError) {
            if (processError == QProcess::FailedToStart)
                qCWarning(SESSION_AUTOSTART) << "autostart failed:" << id;
        });
        QString launchError;
        if (!SupervisedProcessLauncher::start(
                *process, entry.program, entry.arguments, &launchError)) {
            qCWarning(SESSION_AUTOSTART) << "autostart refused:" << id << launchError;
            continue;
        }
        ++m_launchedCount;
        m_processes.push_back(std::move(process));
    }
}

void SessionAutostartRunner::stop() noexcept
{
    // AGENT-GUARD: stop only the QProcess objects this login batch created.
    // Never kill by desktop-entry basename or ambient process name; another
    // session or a user-started instance may have the same program.
    for (auto &process : m_processes) {
        if (process->state() == QProcess::NotRunning)
            continue;
        process->terminate();
        if (!process->waitForFinished(StopTimeoutMilliseconds)) {
            process->kill();
            process->waitForFinished(StopTimeoutMilliseconds);
        }
    }
    m_processes.clear();
    m_started = false;
    m_launchedCount = 0;
}

} // namespace QindaQt::SessionSupervisor
