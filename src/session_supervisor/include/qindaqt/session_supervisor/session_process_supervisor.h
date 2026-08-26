// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

namespace QindaQt::SessionSupervisor {

struct SessionProcessOptions final {
    QString notificationHostExecutable = QStringLiteral("qindaqt-notification-host");
    QString shellExecutable = QStringLiteral("qindaqt-shell");
    QString profileId;
    QString themeId;
};

// Owns exactly the notification host and shell child processes. Unexpected
// exit of either child tears down the sibling and ends the compositor session.
class SessionProcessSupervisor final : public QObject {
    Q_OBJECT

public:
    explicit SessionProcessSupervisor(SessionProcessOptions options,
                                      QObject *parent = nullptr);
    ~SessionProcessSupervisor() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

Q_SIGNALS:
    void finished(int exitCode, const QString &message);

private:
    [[nodiscard]] QString resolveExecutable(const QString &configured) const;
    void childFinished(const QString &role, int exitCode,
                       QProcess::ExitStatus exitStatus);
    static void stopChild(QProcess &process) noexcept;

    SessionProcessOptions m_options;
    QProcess m_host;
    QProcess m_shell;
    bool m_running = false;
    bool m_stopping = false;
};

} // namespace QindaQt::SessionSupervisor
