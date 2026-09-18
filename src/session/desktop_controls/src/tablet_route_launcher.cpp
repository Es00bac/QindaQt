// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/desktop_controls/tablet_route_launcher.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <utility>

namespace QindaQt::Session::DesktopControls {
namespace {

// AGENT-CONTRACT: a bare name resolves sibling-first so an installed session
// never launches a foreign development build from ambient PATH. Same rule as
// ScreenshotLauncher.
QString resolveProgram(const QString &program) {
    if (program.contains(QLatin1Char('/'))) {
        return QFileInfo(program).isExecutable() ? program : QString{};
    }
    const QString sibling =
        QCoreApplication::applicationDirPath() + QLatin1Char('/') + program;
    if (QFileInfo(sibling).isExecutable()) {
        return sibling;
    }
    return QStandardPaths::findExecutable(program);
}

} // namespace

TabletRouteLauncher::TabletRouteLauncher(Launch launch, QObject *parent)
    : QObject(parent), m_launch(std::move(launch)) {
    if (!m_launch) {
        m_launch = [](const QString &program, const QStringList &arguments) {
            const QString resolved = resolveProgram(program);
            if (resolved.isEmpty()) {
                return false;
            }
            return QProcess::startDetached(resolved, arguments);
        };
    }
}

TabletRouteLauncher::~TabletRouteLauncher() = default;

QString TabletRouteLauncher::settingsProgram() {
    return QStringLiteral("qindaqt-settings");
}

QStringList TabletRouteLauncher::argumentsFor(const QString &deviceGroupId) {
    QStringList arguments{QStringLiteral("--page"), QStringLiteral("input"),
                          QStringLiteral("--destination"),
                          QStringLiteral("tablet")};
    if (!deviceGroupId.isEmpty()) {
        arguments << QStringLiteral("--select") << deviceGroupId;
    }
    return arguments;
}

void TabletRouteLauncher::openTabletSettings(const QString &deviceGroupId) {
    if (!m_launch(settingsProgram(), argumentsFor(deviceGroupId))) {
        Q_EMIT launchFailed(QStringLiteral("tablet-settings-unavailable"));
    }
}

} // namespace QindaQt::Session::DesktopControls
