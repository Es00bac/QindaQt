// SPDX-License-Identifier: LGPL-3.0-or-later
#include "first_launch_welcome.h"

#include "qindaqt/session_supervisor/supervised_process_launcher.h"

#include <QFileInfo>

namespace QindaQt::SessionSupervisor {
namespace {
constexpr int StopTimeoutMilliseconds = 2'000;
}

FirstLaunchWelcome::FirstLaunchWelcome() {
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);
    QObject::connect(&m_process, &QProcess::started, &m_process,
                     [this] { m_processId = m_process.processId(); });
    QObject::connect(&m_process, &QProcess::finished, &m_process,
                     [this](int, QProcess::ExitStatus) { m_processId = 0; });
    QObject::connect(&m_process, &QProcess::errorOccurred, &m_process,
                     [this](QProcess::ProcessError error) {
                         if (error == QProcess::FailedToStart) {
                             m_processId = 0;
                         }
                     });
}

FirstLaunchWelcome::~FirstLaunchWelcome() { stop(); }

void FirstLaunchWelcome::start(const QString &program) {
    // Optional autostart is sibling-only when the configured name is bare.
    // This avoids executing an unrelated PATH entry in a partial installation.
    if (isRunning() || !QFileInfo(program).isExecutable()) {
        return;
    }
    QString ignored;
    if (!SupervisedProcessLauncher::start(
            m_process, program, {QStringLiteral("--first-launch")}, &ignored)) {
        m_processId = 0;
    }
}

void FirstLaunchWelcome::stop() noexcept {
    if (m_process.state() == QProcess::NotRunning) {
        m_processId = 0;
        return;
    }
    m_process.terminate();
    if (!m_process.waitForFinished(StopTimeoutMilliseconds)) {
        m_process.kill();
        m_process.waitForFinished(StopTimeoutMilliseconds);
    }
    m_processId = 0;
}

bool FirstLaunchWelcome::isRunning() const noexcept {
    return m_process.state() != QProcess::NotRunning;
}

qint64 FirstLaunchWelcome::processId() const noexcept {
    return isRunning() ? m_processId : 0;
}

} // namespace QindaQt::SessionSupervisor
