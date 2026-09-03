// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QProcess;
class QProcessEnvironment;

namespace QindaQt::Test::PanelVisibilityCapture {

// Configures one not-yet-started screenshot child. The caller retains process
// ownership and must provide the already-authenticated parent Weston loader
// path; invalid inputs leave the process unconfigured.
[[nodiscard]] bool configureCaptureProcess(
    QProcess &process, const QProcessEnvironment &baseEnvironment,
    const QString &tool, const QString &libraryPath,
    QString *failure = nullptr);

} // namespace QindaQt::Test::PanelVisibilityCapture
