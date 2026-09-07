// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/screenshot_launcher.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <utility>

namespace QindaQt::Session::DesktopControls {

ScreenshotLauncher::ScreenshotLauncher(QString program, QStringList arguments,
                                       QObject *parent)
    : QObject(parent)
    , m_program(std::move(program))
    , m_arguments(std::move(arguments))
{
}

ScreenshotLauncher::~ScreenshotLauncher() = default;

QString ScreenshotLauncher::resolveProgram() const
{
    if (m_program.trimmed().isEmpty()) {
        return {};
    }
    if (m_program.contains(QLatin1Char('/'))) {
        return QFileInfo(m_program).isExecutable() ? m_program : QString{};
    }
    // AGENT-CONTRACT: a bare name resolves sibling-first so an installed
    // session never launches a foreign development build from ambient PATH.
    const QString sibling = QCoreApplication::applicationDirPath()
        + QLatin1Char('/') + m_program;
    if (QFileInfo(sibling).isExecutable()) {
        return sibling;
    }
    return QStandardPaths::findExecutable(m_program);
}

void ScreenshotLauncher::launch()
{
    const QString resolved = resolveProgram();
    if (resolved.isEmpty()) {
        Q_EMIT launchFailed(QStringLiteral("screenshot-program-unavailable"));
        return;
    }
    qint64 processId = 0;
    if (!QProcess::startDetached(resolved, m_arguments, QString{}, &processId)) {
        Q_EMIT launchFailed(QStringLiteral("screenshot-launch-failed"));
        return;
    }
    Q_EMIT launchStarted(processId);
}

} // namespace QindaQt::Session::DesktopControls
