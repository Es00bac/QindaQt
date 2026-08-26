// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QtTypes>

namespace QindaQt::AppletHost {

struct LifecyclePolicy final {
    qint64 baseBackoffMs = 250;
    qint64 maximumBackoffMs = 30'000;
    qint64 stableRuntimeMs = 60'000;
    int crashLimit = 5;

    [[nodiscard]] QString validationError() const;
};

enum class LifecycleState {
    Stopped,
    Starting,
    Handshaking,
    Running,
    Stopping,
    Backoff,
    Disabled,
};

enum class LifecycleAction {
    None,
    LaunchProcess,
    AwaitHandshake,
    TerminateProcess,
    ScheduleRestart,
};

enum class ProcessExitCause {
    Crash,
    LaunchFailure,
    UnexpectedCleanExit,
};

struct LifecycleTransition final {
    bool accepted = false;
    LifecycleState state = LifecycleState::Stopped;
    LifecycleAction action = LifecycleAction::None;
    qint64 retryAtMs = -1;
    QString message;
};

class HostLifecycle final {
public:
    explicit HostLifecycle(LifecyclePolicy policy = {});

    [[nodiscard]] LifecycleState state() const;
    [[nodiscard]] int consecutiveCrashes() const;
    [[nodiscard]] qint64 retryAtMs() const;

    [[nodiscard]] LifecycleTransition requestStart(qint64 monotonicNowMs);
    [[nodiscard]] LifecycleTransition processStarted();
    [[nodiscard]] LifecycleTransition handshakeAccepted(qint64 monotonicNowMs);
    [[nodiscard]] LifecycleTransition handshakeRejected(const QString &reason);
    [[nodiscard]] LifecycleTransition requestStop();
    [[nodiscard]] LifecycleTransition processExited(ProcessExitCause cause,
                                                    qint64 monotonicNowMs);
    [[nodiscard]] LifecycleTransition resetDisabled();

private:
    [[nodiscard]] LifecycleTransition transition(bool accepted,
                                                 LifecycleAction action,
                                                 const QString &message) const;
    [[nodiscard]] LifecycleTransition scheduleAfterFailure(ProcessExitCause cause,
                                                           qint64 monotonicNowMs);
    [[nodiscard]] qint64 backoffForCrashCount(int crashCount) const;

    LifecyclePolicy m_policy;
    QString m_policyError;
    LifecycleState m_state = LifecycleState::Stopped;
    int m_consecutiveCrashes = 0;
    qint64 m_retryAtMs = -1;
    qint64 m_runningSinceMs = -1;
    bool m_disableAfterStop = false;
    QString m_disableReason;
};

[[nodiscard]] QString toString(LifecycleState value);
[[nodiscard]] QString toString(LifecycleAction value);

} // namespace QindaQt::AppletHost
