// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsroutelauncher.h"

#include <QProcess>

#include <utility>

namespace QindaQt::Shell {

SettingsRouteLauncher::SettingsRouteLauncher(Launch launch, QObject *parent)
    : QObject(parent), m_launch(std::move(launch))
{
    if (!m_launch) {
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
