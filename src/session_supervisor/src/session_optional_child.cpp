// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session_supervisor/session_optional_child.h"

#include "qindaqt/session_supervisor/supervised_process_launcher.h"

#include <QFileInfo>

#include <utility>

namespace QindaQt::SessionSupervisor {
namespace {

constexpr int StopTimeoutMilliseconds = 2'000;

} // namespace

OptionalSessionChild::OptionalSessionChild(QString role, QStringList arguments,
                                           QObject *parent)
    : QObject(parent)
    , m_role(std::move(role))
    , m_arguments(std::move(arguments))
{
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);
    connect(&m_process, &QProcess::finished, this, [this] { ended(); });
    connect(&m_process, &QProcess::started, this, [this] {
        m_processId = m_process.processId();
        if (m_restartCount > 0 && m_previousProcessId > 1) {
            Q_EMIT restarted(m_role, m_previousProcessId, m_processId);
            m_previousProcessId = 0;
        }
    });
    connect(&m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    ended();
                }
            });
}

OptionalSessionChild::~OptionalSessionChild() { stop(); }

void OptionalSessionChild::start(const QString &resolvedProgram)
{
    if (isRunning() || resolvedProgram.isEmpty()
        || !QFileInfo(resolvedProgram).isExecutable()) {
        return;
    }
    QString ignored;
    if (!SupervisedProcessLauncher::start(m_process, resolvedProgram, m_arguments,
                                          &ignored)) {
        m_processId = 0;
    }
}

void OptionalSessionChild::stop() noexcept
{
    if (m_process.state() == QProcess::NotRunning) {
        m_processId = 0;
        return;
    }
    m_stopping = true;
    Q_EMIT stopRequested(m_role);
    m_process.terminate();
    if (!m_process.waitForFinished(StopTimeoutMilliseconds)) {
        m_process.kill();
        m_process.waitForFinished(StopTimeoutMilliseconds);
    }
    m_stopping = false;
    m_processId = 0;
}

bool OptionalSessionChild::isRunning() const noexcept
{
    return m_process.state() != QProcess::NotRunning;
}

qint64 OptionalSessionChild::processId() const noexcept
{
    return isRunning() ? m_processId : 0;
}

int OptionalSessionChild::restartCount() const noexcept
{
    return m_restartCount;
}

void OptionalSessionChild::ended()
{
    if (m_stopping || m_restartCount >= m_restartLimit) {
        m_processId = 0;
        return;
    }
    m_previousProcessId = m_processId;
    ++m_restartCount;
    m_processId = 0;
    const QString program = m_process.program();
    if (!program.isEmpty()) {
        start(program);
    }
}

} // namespace QindaQt::SessionSupervisor
