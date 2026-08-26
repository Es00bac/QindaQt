// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session_supervisor/session_process_supervisor.h"

#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/session_supervisor/tokenized_process_launcher.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace QindaQt::SessionSupervisor {
namespace {

constexpr int StopTimeoutMilliseconds = 2'000;

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

SessionProcessSupervisor::SessionProcessSupervisor(
    SessionProcessOptions options, QObject *parent)
    : QObject(parent)
    , m_options(std::move(options))
{
    m_host.setProcessChannelMode(QProcess::ForwardedChannels);
    m_shell.setProcessChannelMode(QProcess::ForwardedChannels);
    connect(&m_host, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus status) {
                childFinished(QStringLiteral("notification host"), code, status);
            });
    connect(&m_shell, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus status) {
                childFinished(QStringLiteral("shell"), code, status);
            });
}

SessionProcessSupervisor::~SessionProcessSupervisor()
{
    stop();
}

bool SessionProcessSupervisor::start(QString *error)
{
    if (m_running || m_stopping) {
        setError(error, QStringLiteral("QindaQt session supervisor is already active"));
        return false;
    }
    const auto token =
        Services::NotificationPresentation::PresentationAccessToken::generate();
    const QString hostProgram = resolveExecutable(m_options.notificationHostExecutable);
    if (!TokenizedProcessLauncher::start(m_host, hostProgram, {}, token, error)) {
        return false;
    }

    QStringList shellArguments;
    if (!m_options.profileId.isEmpty()) {
        shellArguments.append({QStringLiteral("--profile"), m_options.profileId});
    }
    if (!m_options.themeId.isEmpty()) {
        shellArguments.append({QStringLiteral("--theme"), m_options.themeId});
    }
    const QString shellProgram = resolveExecutable(m_options.shellExecutable);
    if (!TokenizedProcessLauncher::start(m_shell, shellProgram, shellArguments,
                                         token, error)) {
        m_stopping = true;
        stopChild(m_host);
        m_stopping = false;
        return false;
    }
    m_running = true;
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
    stopChild(m_shell);
    stopChild(m_host);
    m_stopping = false;
}

bool SessionProcessSupervisor::isRunning() const noexcept
{
    return m_running && m_host.state() != QProcess::NotRunning &&
        m_shell.state() != QProcess::NotRunning;
}

QString SessionProcessSupervisor::resolveExecutable(const QString &configured) const
{
    const QFileInfo requested(configured);
    if (requested.isAbsolute() || configured.contains(QLatin1Char('/'))) {
        return configured;
    }
    const QString sibling =
        QCoreApplication::applicationDirPath() + QLatin1Char('/') + configured;
    return QFileInfo(sibling).isExecutable() ? sibling : configured;
}

void SessionProcessSupervisor::childFinished(
    const QString &role, int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_running || m_stopping) {
        return;
    }
    m_stopping = true;
    m_running = false;
    if (role == QLatin1String("notification host")) {
        stopChild(m_shell);
    } else {
        stopChild(m_host);
    }
    m_stopping = false;
    const bool clean = exitStatus == QProcess::NormalExit && exitCode == 0;
    Q_EMIT finished(
        clean ? 1 : std::max(exitCode, 1),
        QStringLiteral("%1 exited %2")
            .arg(role, clean ? QStringLiteral("unexpectedly")
                             : QStringLiteral("with a failure")));
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
