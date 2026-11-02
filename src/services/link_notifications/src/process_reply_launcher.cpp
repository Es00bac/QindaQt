// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/process_reply_launcher.h>

#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>

namespace QindaQt::Link {

bool ProcessReplyLauncher::available() const
{
    return !QStandardPaths::locate(QStandardPaths::ApplicationsLocation, m_desktopFile).isEmpty();
}

void ProcessReplyLauncher::launch(const QString &deviceId, const QString &key, const QString &actionId)
{
    QProcess::startDetached(m_program, {QStringLiteral("--reply"), deviceId, key, actionId});
}

} // namespace QindaQt::Link
