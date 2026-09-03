// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitycaptureprocess.h"

#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

#include <algorithm>
#include <utility>

namespace QindaQt::Test::PanelVisibilityCapture {
namespace {

bool fail(QString *failure, QString message)
{
    if (failure != nullptr) {
        *failure = std::move(message);
    }
    return false;
}

bool isAbsoluteSearchPath(const QString &path)
{
    const QStringList entries = path.split(QLatin1Char(':'), Qt::KeepEmptyParts);
    return !path.isEmpty() && std::all_of(
        entries.cbegin(), entries.cend(), [](const QString &entry) {
            return !entry.isEmpty() && QFileInfo(entry).isAbsolute();
        });
}

} // namespace

bool configureCaptureProcess(
    QProcess &process, const QProcessEnvironment &baseEnvironment,
    const QString &tool, const QString &libraryPath, QString *failure)
{
    if (process.state() != QProcess::NotRunning || tool.trimmed().isEmpty()) {
        return fail(failure, QStringLiteral("capture process request is invalid"));
    }
    if (!isAbsoluteSearchPath(libraryPath)) {
        return fail(failure, QStringLiteral("capture loader path is not exact and absolute"));
    }
    QProcessEnvironment environment = baseEnvironment;
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"),
                       QStringLiteral("qindaqt-parent-wayland"));
    const QString inherited = environment.value(QStringLiteral("LD_LIBRARY_PATH"));
    environment.insert(
        QStringLiteral("LD_LIBRARY_PATH"),
        inherited.isEmpty() ? libraryPath
                            : QStringLiteral("%1:%2").arg(libraryPath, inherited));
    // AGENT-GUARD: This setter is the C++ half of the mixed-ABI boundary.
    // Omitting it silently runs weston-screenshooter with system KWin's loader
    // closure even when Python supplied the authenticated Weston path.
    process.setProcessEnvironment(environment);
    process.setProgram(tool);
    if (failure != nullptr) {
        failure->clear();
    }
    return true;
}

} // namespace QindaQt::Test::PanelVisibilityCapture
