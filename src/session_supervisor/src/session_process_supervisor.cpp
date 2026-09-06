// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session_supervisor/session_process_supervisor.h"

#include "qindaqt/session_supervisor/direct_parent_process.h"
#include "qindaqt/session_supervisor/supervised_process_launcher.h"
#include "qindaqt/session_supervisor/tokenized_process_launcher.h"
#include "first_launch_welcome.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace QindaQt::SessionSupervisor {
namespace {

constexpr int StopTimeoutMilliseconds = 2'000;
constexpr int ShellRestartLimit = 1;
constexpr int SecretAgentRestartLimit = 1;

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

std::optional<QStringList> shellProcessArguments(const SessionProcessOptions &options,
                                                 QString *error)
{
    if (!isUsableCompositorProcessId(options.compositorProcessId)) {
        setError(error, QStringLiteral("expected compositor process id is invalid"));
        return std::nullopt;
    }
    QStringList arguments;
    if (!options.profileId.isEmpty()) {
        arguments.append({QStringLiteral("--profile"), options.profileId});
    }
    if (!options.themeId.isEmpty()) {
        arguments.append({QStringLiteral("--theme"), options.themeId});
    }
    arguments.append(
        {QStringLiteral("--compositor-pid"), QString::number(options.compositorProcessId)});
    setError(error, {});
    return arguments;
}

SessionProcessSupervisor::SessionProcessSupervisor(SessionProcessOptions options, QObject *parent)
    : QObject(parent), m_options(std::move(options)),
      m_welcome(std::make_unique<FirstLaunchWelcome>())
{
    m_host.setProcessChannelMode(QProcess::ForwardedChannels);
    m_shell.setProcessChannelMode(QProcess::ForwardedChannels);
    m_networkSecretAgent.setProcessChannelMode(QProcess::ForwardedChannels);
    connect(&m_host, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        childFinished(ChildRole::NotificationHost, code, status);
    });
    connect(&m_shell, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        childFinished(ChildRole::Shell, code, status);
    });
    connect(&m_networkSecretAgent, &QProcess::finished, this,
            [this](int, QProcess::ExitStatus) { networkSecretAgentEnded(); });
    connect(&m_networkSecretAgent, &QProcess::started, this, [this] {
        m_networkSecretAgentProcessId = m_networkSecretAgent.processId();
        if (m_networkSecretAgentRestartCount > 0
            && m_networkSecretAgentPreviousProcessId > 1) {
            Q_EMIT networkSecretAgentRestarted(
                m_networkSecretAgentPreviousProcessId,
                m_networkSecretAgentProcessId);
            m_networkSecretAgentPreviousProcessId = 0;
        }
    });
    connect(&m_networkSecretAgent, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    networkSecretAgentEnded();
                }
            });
}

SessionProcessSupervisor::~SessionProcessSupervisor() { stop(); }

bool SessionProcessSupervisor::start(QString *error)
{
    if (m_running || m_stopping) {
        setError(error, QStringLiteral("QindaQt session supervisor is already active"));
        return false;
    }
    if (!shellProcessArguments(m_options, error).has_value()) {
        return false;
    }
    m_shellRestartCount = 0;
    m_networkSecretAgentRestartCount = 0;
    m_hostProcessId = 0;
    m_shellProcessId = 0;
    m_networkSecretAgentProcessId = 0;
    m_networkSecretAgentPreviousProcessId = 0;
    m_token = Services::NotificationPresentation::PresentationAccessToken::generate();
    const QString hostProgram = resolveExecutable(m_options.notificationHostExecutable);
    if (!TokenizedProcessLauncher::start(m_host, hostProgram, {}, *m_token, error)) {
        m_token.reset();
        return false;
    }
    m_hostProcessId = m_host.processId();

    if (!startShell(error)) {
        m_stopping = true;
        stopChild(m_host);
        m_hostProcessId = 0;
        m_token.reset();
        m_stopping = false;
        return false;
    }
    m_running = true;
    startNetworkSecretAgent();
    startWelcome();
    setError(error, {});
    return true;
}

void SessionProcessSupervisor::stop() noexcept
{
    if (m_stopping) {
        return;
    }
    m_stopping = true;
    m_running = false;
    // AGENT-GUARD: Clear running state and hold m_stopping across both waits so
    // reentrant finished signals cannot launch a replacement. The restart
    // count is reset only after both children are stopped for the next session.
    m_token.reset();
    if (m_welcome->isRunning()) {
        Q_EMIT childStopRequested(QStringLiteral("welcome"));
    }
    m_welcome->stop();
    if (m_shell.state() != QProcess::NotRunning) {
        Q_EMIT childStopRequested(QStringLiteral("shell"));
    }
    stopChild(m_shell);
    if (m_host.state() != QProcess::NotRunning) {
        Q_EMIT childStopRequested(QStringLiteral("notification-host"));
    }
    stopChild(m_host);
    if (m_networkSecretAgent.state() != QProcess::NotRunning) {
        Q_EMIT childStopRequested(QStringLiteral("network-secret-agent"));
    }
    stopChild(m_networkSecretAgent);
    m_shellProcessId = 0;
    m_hostProcessId = 0;
    m_networkSecretAgentProcessId = 0;
    m_networkSecretAgentPreviousProcessId = 0;
    m_shellRestartCount = 0;
    m_stopping = false;
}

void SessionProcessSupervisor::requestLogout()
{
    if (!canLogout()) {
        return;
    }
    stop();
    Q_EMIT finished(0, QStringLiteral("logout requested"));
}

bool SessionProcessSupervisor::isRunning() const noexcept
{
    return m_running && m_host.state() != QProcess::NotRunning
           && m_shell.state() != QProcess::NotRunning;
}

bool SessionProcessSupervisor::canLogout() const noexcept
{
    return isRunning() && !m_stopping;
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

qint64 SessionProcessSupervisor::welcomeProcessId() const noexcept
{
    return m_welcome->processId();
}

QString SessionProcessSupervisor::resolveExecutable(const QString &configured) const
{
    const QFileInfo requested(configured);
    if (requested.isAbsolute() || configured.contains(QLatin1Char('/'))) {
        return configured;
    }
    const QString sibling = QCoreApplication::applicationDirPath() + QLatin1Char('/') + configured;
    return QFileInfo(sibling).isExecutable() ? sibling : configured;
}

bool SessionProcessSupervisor::startShell(QString *error, qint64 predecessorProcessId)
{
    if (!m_token.has_value()) {
        setError(error, QStringLiteral("presentation token is unavailable"));
        return false;
    }
    auto arguments = shellProcessArguments(m_options, error);
    if (!arguments.has_value()) {
        return false;
    }
    if (predecessorProcessId > 1
        && qEnvironmentVariable("QINDAQT_DEVELOPMENT_CONTROL") == QLatin1String("1")) {
        // This non-secret lineage hint exists only for the private live-test
        // shell. Ordinary production replacements retain their exact argv.
        arguments->append({QStringLiteral("--development-evidence-predecessor-pid"),
                           QString::number(predecessorProcessId)});
    }
    const QString program = resolveExecutable(m_options.shellExecutable);
    if (!TokenizedProcessLauncher::start(m_shell, program, *arguments, *m_token, error)) {
        m_shellProcessId = 0;
        return false;
    }
    m_shellProcessId = m_shell.processId();
    return true;
}

void SessionProcessSupervisor::startNetworkSecretAgent()
{
    QString program = resolveExecutable(m_options.networkSecretAgentExecutable);
    // Optional autostart is intentionally sibling-only for a bare production
    // name. Searching ambient PATH could launch a foreign development build.
    if (program.isEmpty() || !QFileInfo(program).isExecutable()) {
        m_networkSecretAgentProcessId = 0;
        return;
    }
    QString ignored;
    if (!SupervisedProcessLauncher::start(m_networkSecretAgent, program, {}, &ignored)) {
        m_networkSecretAgentProcessId = 0;
        return;
    }
}

void SessionProcessSupervisor::startWelcome()
{
    // AGENT-CONTRACT: Launch only after the essential shell successfully
    // starts. The executable owns its show-next-launch decision; the session
    // supervisor owns only this optional child's lifetime.
    m_welcome->start(resolveExecutable(m_options.welcomeExecutable));
}

void SessionProcessSupervisor::networkSecretAgentEnded()
{
    if (!m_running || m_stopping
        || m_networkSecretAgentRestartCount >= SecretAgentRestartLimit) {
        m_networkSecretAgentProcessId = 0;
        return;
    }
    const qint64 previousProcessId = m_networkSecretAgentProcessId;
    ++m_networkSecretAgentRestartCount;
    m_networkSecretAgentProcessId = 0;
    m_networkSecretAgentPreviousProcessId = previousProcessId;
    startNetworkSecretAgent();
}

void SessionProcessSupervisor::childFinished(ChildRole role, int exitCode,
                                             QProcess::ExitStatus exitStatus)
{
    if (!m_running || m_stopping) {
        return;
    }

    if (role == ChildRole::Shell && m_shellRestartCount < ShellRestartLimit
        && m_host.state() != QProcess::NotRunning) {
        const qint64 previousProcessId = m_shellProcessId;
        ++m_shellRestartCount;
        QString error;
        // AGENT-CONTRACT: A replacement reuses only the in-memory session
        // token. TokenizedProcessLauncher creates a new one-shot descriptor,
        // and shellProcessArguments repeats the same authenticated KWin PID.
        if (startShell(&error, previousProcessId)) {
            Q_EMIT shellRestarted(previousProcessId, m_shellProcessId);
            return;
        }
        finishSession(role, exitCode, exitStatus,
                      QStringLiteral("could not restart shell: %1").arg(error));
        return;
    }

    finishSession(role, exitCode, exitStatus);
}

void SessionProcessSupervisor::finishSession(ChildRole role, int exitCode,
                                             QProcess::ExitStatus exitStatus, const QString &detail)
{
    m_stopping = true;
    m_running = false;
    m_token.reset();
    if (m_welcome->isRunning()) {
        Q_EMIT childStopRequested(QStringLiteral("welcome"));
    }
    m_welcome->stop();
    if (role == ChildRole::NotificationHost) {
        if (m_shell.state() != QProcess::NotRunning) {
            Q_EMIT childStopRequested(QStringLiteral("shell"));
        }
        stopChild(m_shell);
        m_shellProcessId = 0;
    } else {
        if (m_host.state() != QProcess::NotRunning) {
            Q_EMIT childStopRequested(QStringLiteral("notification-host"));
        }
        stopChild(m_host);
        m_hostProcessId = 0;
    }
    if (m_networkSecretAgent.state() != QProcess::NotRunning) {
        Q_EMIT childStopRequested(QStringLiteral("network-secret-agent"));
    }
    stopChild(m_networkSecretAgent);
    m_networkSecretAgentProcessId = 0;
    if (role == ChildRole::NotificationHost) {
        m_hostProcessId = 0;
    } else {
        m_shellProcessId = 0;
    }
    m_stopping = false;
    const bool clean = exitStatus == QProcess::NormalExit && exitCode == 0;
    const QString roleName = role == ChildRole::NotificationHost
                                 ? QStringLiteral("notification host")
                                 : QStringLiteral("shell");
    const QString reason = detail.isEmpty()
                               ? QStringLiteral("%1 exited %2")
                                     .arg(roleName, clean ? QStringLiteral("unexpectedly")
                                                          : QStringLiteral("with a failure"))
                               : detail;
    Q_EMIT finished(clean ? 1 : std::max(exitCode, 1), reason);
}

void SessionProcessSupervisor::stopChild(QProcess &process) noexcept
{
    if (process.state() == QProcess::NotRunning) {
        return;
    }
    process.terminate();
    if (!process.waitForFinished(StopTimeoutMilliseconds)) {
        process.kill();
        process.waitForFinished(StopTimeoutMilliseconds);
    }
}

} // namespace QindaQt::SessionSupervisor
