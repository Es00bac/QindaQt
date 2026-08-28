// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_access_token.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace QindaQt::SessionSupervisor {

struct SessionProcessOptions final {
    QString notificationHostExecutable = QStringLiteral("qindaqt-notification-host");
    QString shellExecutable = QStringLiteral("qindaqt-shell");
    QString profileId;
    QString themeId;
    qint64 compositorProcessId = 0;
};

// Builds the non-secret portion of the shell process contract. The tokenized
// launcher appends its inherited descriptor after this exact argument list.
[[nodiscard]] std::optional<QStringList> shellProcessArguments(
    const SessionProcessOptions &options, QString *error = nullptr);

// Owns exactly the notification host and shell child processes. The host is
// session-resident; one unexpected shell exit consumes the bounded recovery
// budget and starts a replacement with a fresh token descriptor. Host exit,
// replacement failure, or a second shell exit ends the compositor session.
class SessionProcessSupervisor final : public QObject {
    Q_OBJECT

public:
    explicit SessionProcessSupervisor(SessionProcessOptions options,
                                      QObject *parent = nullptr);
    ~SessionProcessSupervisor() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

    // Read-only process diagnostics never transfer ownership. A returned PID
    // is valid only while the corresponding child remains supervised.
    [[nodiscard]] qint64 notificationHostProcessId() const noexcept;
    [[nodiscard]] qint64 shellProcessId() const noexcept;
    [[nodiscard]] int shellRestartCount() const noexcept;

Q_SIGNALS:
    void shellRestarted(qint64 previousProcessId, qint64 processId);
    void finished(int exitCode, const QString &message);

private:
    enum class ChildRole {
        NotificationHost,
        Shell,
    };

    [[nodiscard]] QString resolveExecutable(const QString &configured) const;
    [[nodiscard]] bool startShell(QString *error,
                                  qint64 predecessorProcessId = 0);
    void childFinished(ChildRole role, int exitCode,
                       QProcess::ExitStatus exitStatus);
    void finishSession(ChildRole role, int exitCode,
                       QProcess::ExitStatus exitStatus,
                       const QString &detail = {});
    static void stopChild(QProcess &process) noexcept;

    SessionProcessOptions m_options;
    QProcess m_host;
    QProcess m_shell;
    std::optional<Services::NotificationPresentation::PresentationAccessToken>
        m_token;
    qint64 m_hostProcessId = 0;
    qint64 m_shellProcessId = 0;
    int m_shellRestartCount = 0;
    bool m_running = false;
    bool m_stopping = false;
};

} // namespace QindaQt::SessionSupervisor
