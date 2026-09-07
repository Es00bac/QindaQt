// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Session::DesktopControls {

// Launches the configured screenshot helper detached so it survives this
// process. A bare program name is resolved only against this process's
// sibling directory and the ambient PATH; an unresolvable program is honest
// failure feedback, never a silent no-op.
class ScreenshotLauncher final : public QObject {
    Q_OBJECT

public:
    ScreenshotLauncher(QString program, QStringList arguments,
                       QObject *parent = nullptr);
    ~ScreenshotLauncher() override;

    ScreenshotLauncher(const ScreenshotLauncher &) = delete;
    ScreenshotLauncher &operator=(const ScreenshotLauncher &) = delete;

    void launch();

    [[nodiscard]] QString program() const noexcept { return m_program; }
    [[nodiscard]] QStringList arguments() const noexcept { return m_arguments; }

Q_SIGNALS:
    void launchStarted(qint64 processId);
    void launchFailed(const QString &reasonCode);

private:
    [[nodiscard]] QString resolveProgram() const;

    QString m_program;
    QStringList m_arguments;
};

} // namespace QindaQt::Session::DesktopControls
