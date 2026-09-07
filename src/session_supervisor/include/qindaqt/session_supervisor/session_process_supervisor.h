// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_access_token.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QtTypes>

#include <optional>
#include <memory>

namespace QindaQt::SessionSupervisor {

class FirstLaunchWelcome;
class OptionalSessionChild;

struct SessionProcessOptions final {
    QString notificationHostExecutable = QStringLiteral("qindaqt-notification-host");
    QString shellExecutable = QStringLiteral("qindaqt-shell");
    QString networkSecretAgentExecutable =
        QStringLiteral("qindaqt-network-secret-agent");
    QString welcomeExecutable = QStringLiteral("qindaqt-welcome");
    // Media keys, screenshot launch, and idle display-off. Started after the
    // shell so the compositor's global-shortcut service already exists.
    QString desktopControlsExecutable = QStringLiteral("qindaqt-desktop-controls");
    // Authentication-agent helper for polkit privilege prompts. An empty value
    // means the session has no agent to offer; the caller resolves known
    // distribution paths before constructing the options.
    QString polkitAgentExecutable;
    QString profileId;
    QString themeId;
    qint64 compositorProcessId = 0;
};

// Builds the non-secret portion of the shell process contract. The tokenized
// launcher appends its inherited descriptor after this exact argument list.
[[nodiscard]] std::optional<QStringList> shellProcessArguments(
    const SessionProcessOptions &options, QString *error = nullptr);

// Owns the essential notification host and shell plus optional installed
// network-secret-agent, desktop-controls, polkit-agent and first-launch
// Welcome children. The host is
// session-resident; an unexpected shell exit schedules a paced replacement
// with a fresh token descriptor while the host remains healthy. Retry delay is
// bounded and resets after a stable shell run. Each optional agent has an
// independent one-restart budget and never participates in readiness. Welcome
// starts once after the shell, never restarts, and cannot end the session.
// Host exit, explicit stop, and supervisor/compositor death still end the
// complete session; repeated shell failures alone do not.
class SessionProcessSupervisor final : public QObject {
    Q_OBJECT

public:
    explicit SessionProcessSupervisor(SessionProcessOptions options,
                                      QObject *parent = nullptr);
    ~SessionProcessSupervisor() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop() noexcept;
    void requestLogout();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] bool canLogout() const noexcept;

    // Read-only process diagnostics never transfer ownership. A returned PID
    // is valid only while the corresponding child remains supervised.
    [[nodiscard]] qint64 notificationHostProcessId() const noexcept;
    [[nodiscard]] qint64 shellProcessId() const noexcept;
    [[nodiscard]] int shellRestartCount() const noexcept;
    [[nodiscard]] qint64 networkSecretAgentProcessId() const noexcept;
    [[nodiscard]] int networkSecretAgentRestartCount() const noexcept;
    [[nodiscard]] qint64 desktopControlsProcessId() const noexcept;
    [[nodiscard]] int desktopControlsRestartCount() const noexcept;
    [[nodiscard]] qint64 polkitAgentProcessId() const noexcept;
    [[nodiscard]] int polkitAgentRestartCount() const noexcept;
    [[nodiscard]] qint64 welcomeProcessId() const noexcept;

Q_SIGNALS:
    void shellRestarted(qint64 previousProcessId, qint64 processId);
    void networkSecretAgentRestarted(qint64 previousProcessId,
                                     qint64 processId);
    void desktopControlsRestarted(qint64 previousProcessId, qint64 processId);
    void polkitAgentRestarted(qint64 previousProcessId, qint64 processId);
    void childStopRequested(const QString &role);
    void finished(int exitCode, const QString &message);

private:
    enum class ChildRole {
        NotificationHost,
        Shell,
    };

    [[nodiscard]] QString resolveExecutable(const QString &configured) const;
    [[nodiscard]] bool startShell(QString *error,
                                  qint64 predecessorProcessId = 0);
    void scheduleShellRestart(qint64 predecessorProcessId,
                               const QString &reason);
    void attemptShellRestart();
    void resetShellRestartBackoff();
    void startNetworkSecretAgent();
    void startOptionalChildren();
    void startWelcome();
    void networkSecretAgentEnded();
    void childFinished(ChildRole role, int exitCode,
                       QProcess::ExitStatus exitStatus);
    void shellProcessError(QProcess::ProcessError error);
    void finishSession(ChildRole role, int exitCode,
                       QProcess::ExitStatus exitStatus,
                       const QString &detail = {});
    static void stopChild(QProcess &process) noexcept;

    SessionProcessOptions m_options;
    QProcess m_host;
    QProcess m_shell;
    QProcess m_networkSecretAgent;
    std::unique_ptr<FirstLaunchWelcome> m_welcome;
    std::unique_ptr<OptionalSessionChild> m_desktopControls;
    std::unique_ptr<OptionalSessionChild> m_polkitAgent;
    std::optional<Services::NotificationPresentation::PresentationAccessToken>
        m_token;
    qint64 m_hostProcessId = 0;
    qint64 m_shellProcessId = 0;
    qint64 m_networkSecretAgentProcessId = 0;
    qint64 m_networkSecretAgentPreviousProcessId = 0;
    int m_shellRestartCount = 0;
    int m_shellRestartDelayMilliseconds = 0;
    qint64 m_shellPredecessorProcessId = 0;
    bool m_shellRestartAttemptInProgress = false;
    QTimer m_shellRestartTimer;
    QTimer m_shellStableTimer;
    int m_networkSecretAgentRestartCount = 0;
    bool m_running = false;
    bool m_stopping = false;
};

} // namespace QindaQt::SessionSupervisor
