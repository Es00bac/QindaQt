// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsroutelauncher.h"

#include <QFileInfo>
#include <QProcess>

#include <utility>

namespace QindaQt::Shell {

SettingsRouteLauncher::SettingsRouteLauncher(Launch launch, QObject *parent)
    : QObject(parent), m_launch(std::move(launch))
{
    if (!m_launch) {
        const QString containedProgram =
            qEnvironmentVariable("QINDAQT_NOTIFICATION_LIVE_SETTINGS_APP");
        const bool containedDevelopmentLaunch =
            qEnvironmentVariable("QINDAQT_DEVELOPMENT_CONTROL") == QLatin1String("1")
            && !containedProgram.isEmpty();
        if (containedDevelopmentLaunch) {
            m_containedProcess = std::make_unique<QProcess>();
            m_launch = [this, containedProgram](QString *error) {
                if (m_containedProcess->state() != QProcess::NotRunning) {
                    return true;
                }
                const QFileInfo program(containedProgram);
                if (!program.isAbsolute() || !program.isFile()
                    || !program.isExecutable()) {
                    if (error) {
                        *error = QStringLiteral(
                            "Private Notification Live settings executable is invalid");
                    }
                    return false;
                }
                m_containedProcess->setProgram(program.absoluteFilePath());
                m_containedProcess->setArguments(
                    {QStringLiteral("--page"), QStringLiteral("notifications")});
                m_containedProcess->setProcessChannelMode(QProcess::ForwardedChannels);
                m_containedProcess->start();
                if (!m_containedProcess->waitForStarted(3'000)) {
                    if (error) {
                        *error = QStringLiteral("Could not open Notification settings: %1")
                                     .arg(m_containedProcess->errorString());
                    }
                    return false;
                }
                return true;
            };
        } else {
            m_launch = [](QString *error) {
                const bool started = QProcess::startDetached(
                    QStringLiteral("qindaqt-settings"),
                    {QStringLiteral("--page"), QStringLiteral("notifications")});
                if (!started && error != nullptr) {
                    *error = QStringLiteral("Could not open Notification settings");
                }
                return started;
            };
        }
    }
}

SettingsRouteLauncher::~SettingsRouteLauncher()
{
    // AGENT-GUARD: Notification Live explicitly replaces detached launching
    // with a child in the disposable process group. Reap that child during a
    // shell restart so it cannot outlive the private Wayland/XDG roots.
    if (m_containedProcess && m_containedProcess->state() != QProcess::NotRunning) {
        m_containedProcess->terminate();
        if (!m_containedProcess->waitForFinished(1'000)) {
            m_containedProcess->kill();
            m_containedProcess->waitForFinished(1'000);
        }
    }
}

bool SettingsRouteLauncher::openNotifications()
{
    QString error;
    const bool started = m_launch(&error);
    error = error.left(512);
    if (m_error != error) {
        m_error = std::move(error);
        Q_EMIT errorTextChanged();
    }
    return started;
}

} // namespace QindaQt::Shell
