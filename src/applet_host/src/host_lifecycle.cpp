// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_host/host_lifecycle.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::AppletHost {

QString LifecyclePolicy::validationError() const
{
    if (baseBackoffMs <= 0) {
        return QStringLiteral("baseBackoffMs must be positive");
    }
    if (maximumBackoffMs < baseBackoffMs) {
        return QStringLiteral("maximumBackoffMs must be at least baseBackoffMs");
    }
    if (stableRuntimeMs < 0) {
        return QStringLiteral("stableRuntimeMs must not be negative");
    }
    if (crashLimit <= 0) {
        return QStringLiteral("crashLimit must be positive");
    }
    return {};
}

HostLifecycle::HostLifecycle(LifecyclePolicy policy)
    : m_policy(std::move(policy))
    , m_policyError(m_policy.validationError())
{
    if (!m_policyError.isEmpty()) {
        m_state = LifecycleState::Disabled;
        m_disableReason = m_policyError;
    }
}

LifecycleState HostLifecycle::state() const
{
    return m_state;
}

int HostLifecycle::consecutiveCrashes() const
{
    return m_consecutiveCrashes;
}

qint64 HostLifecycle::retryAtMs() const
{
    return m_retryAtMs;
}

LifecycleTransition HostLifecycle::requestStart(qint64 monotonicNowMs)
{
    if (!m_policyError.isEmpty()) {
        return transition(false, LifecycleAction::None, m_policyError);
    }
    if (monotonicNowMs < 0) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Monotonic time must not be negative"));
    }
    if (m_state == LifecycleState::Backoff && monotonicNowMs < m_retryAtMs) {
        return transition(false, LifecycleAction::ScheduleRestart,
                          QStringLiteral("Restart backoff has not elapsed"));
    }
    if (m_state != LifecycleState::Stopped && m_state != LifecycleState::Backoff) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Host cannot start from state %1").arg(toString(m_state)));
    }

    m_state = LifecycleState::Starting;
    m_retryAtMs = -1;
    return transition(true, LifecycleAction::LaunchProcess,
                      QStringLiteral("Launch requested"));
}

LifecycleTransition HostLifecycle::processStarted()
{
    if (m_state != LifecycleState::Starting) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Process start is unexpected in state %1")
                              .arg(toString(m_state)));
    }
    m_state = LifecycleState::Handshaking;
    return transition(true, LifecycleAction::AwaitHandshake,
                      QStringLiteral("Process started; handshake required"));
}

LifecycleTransition HostLifecycle::handshakeAccepted(qint64 monotonicNowMs)
{
    if (m_state != LifecycleState::Handshaking || monotonicNowMs < 0) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Handshake acceptance is not valid in the current state"));
    }
    m_state = LifecycleState::Running;
    m_runningSinceMs = monotonicNowMs;
    return transition(true, LifecycleAction::None,
                      QStringLiteral("Host is running"));
}

LifecycleTransition HostLifecycle::handshakeRejected(const QString &reason)
{
    if (m_state != LifecycleState::Handshaking) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Handshake rejection is unexpected in state %1")
                              .arg(toString(m_state)));
    }
    m_disableAfterStop = true;
    m_disableReason = reason.trimmed().isEmpty()
        ? QStringLiteral("Host handshake was rejected")
        : reason;
    m_state = LifecycleState::Stopping;
    return transition(true, LifecycleAction::TerminateProcess, m_disableReason);
}

LifecycleTransition HostLifecycle::requestStop()
{
    if (m_state == LifecycleState::Stopped) {
        return transition(true, LifecycleAction::None, QStringLiteral("Host is already stopped"));
    }
    if (m_state == LifecycleState::Backoff) {
        m_state = LifecycleState::Stopped;
        m_retryAtMs = -1;
        m_consecutiveCrashes = 0;
        return transition(true, LifecycleAction::None,
                          QStringLiteral("Pending restart cancelled"));
    }
    if (m_state == LifecycleState::Disabled) {
        return transition(true, LifecycleAction::None,
                          QStringLiteral("Disabled host has no running process"));
    }
    if (m_state == LifecycleState::Stopping) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Host is already stopping"));
    }

    m_disableAfterStop = false;
    m_state = LifecycleState::Stopping;
    return transition(true, LifecycleAction::TerminateProcess,
                      QStringLiteral("Termination requested"));
}

LifecycleTransition HostLifecycle::processExited(ProcessExitCause cause,
                                                  qint64 monotonicNowMs)
{
    if (monotonicNowMs < 0) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Monotonic time must not be negative"));
    }
    if (m_state == LifecycleState::Stopping) {
        m_runningSinceMs = -1;
        if (m_disableAfterStop) {
            m_state = LifecycleState::Disabled;
            m_disableAfterStop = false;
            return transition(true, LifecycleAction::None, m_disableReason);
        }
        m_state = LifecycleState::Stopped;
        m_consecutiveCrashes = 0;
        return transition(true, LifecycleAction::None,
                          QStringLiteral("Host stopped"));
    }
    if (m_state != LifecycleState::Starting && m_state != LifecycleState::Handshaking
        && m_state != LifecycleState::Running) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Process exit is unexpected in state %1")
                              .arg(toString(m_state)));
    }
    return scheduleAfterFailure(cause, monotonicNowMs);
}

LifecycleTransition HostLifecycle::resetDisabled()
{
    if (m_state != LifecycleState::Disabled) {
        return transition(false, LifecycleAction::None,
                          QStringLiteral("Only a disabled host can be reset"));
    }
    if (!m_policyError.isEmpty()) {
        return transition(false, LifecycleAction::None, m_policyError);
    }
    m_state = LifecycleState::Stopped;
    m_consecutiveCrashes = 0;
    m_retryAtMs = -1;
    m_disableReason.clear();
    return transition(true, LifecycleAction::None,
                      QStringLiteral("Disabled host reset"));
}

LifecycleTransition HostLifecycle::transition(bool accepted,
                                               LifecycleAction action,
                                               const QString &message) const
{
    return {.accepted = accepted,
            .state = m_state,
            .action = action,
            .retryAtMs = m_retryAtMs,
            .message = message};
}

LifecycleTransition HostLifecycle::scheduleAfterFailure(ProcessExitCause cause,
                                                         qint64 monotonicNowMs)
{
    if (m_state == LifecycleState::Running && m_runningSinceMs >= 0
        && monotonicNowMs >= m_runningSinceMs
        && monotonicNowMs - m_runningSinceMs >= m_policy.stableRuntimeMs) {
        m_consecutiveCrashes = 0;
    }
    m_runningSinceMs = -1;
    ++m_consecutiveCrashes;
    if (m_consecutiveCrashes >= m_policy.crashLimit) {
        m_state = LifecycleState::Disabled;
        m_retryAtMs = -1;
        return transition(true, LifecycleAction::None,
                          QStringLiteral("Host disabled after %1 consecutive failures")
                              .arg(m_consecutiveCrashes));
    }

    const qint64 delay = backoffForCrashCount(m_consecutiveCrashes);
    const qint64 maximumTime = std::numeric_limits<qint64>::max();
    m_retryAtMs = monotonicNowMs > maximumTime - delay ? maximumTime
                                                       : monotonicNowMs + delay;
    m_state = LifecycleState::Backoff;
    const QString causeName = cause == ProcessExitCause::Crash
        ? QStringLiteral("crash")
        : cause == ProcessExitCause::LaunchFailure ? QStringLiteral("launch failure")
                                                  : QStringLiteral("unexpected clean exit");
    return transition(true, LifecycleAction::ScheduleRestart,
                      QStringLiteral("Host %1; restart delayed by %2 ms")
                          .arg(causeName)
                          .arg(delay));
}

qint64 HostLifecycle::backoffForCrashCount(int crashCount) const
{
    qint64 delay = m_policy.baseBackoffMs;
    for (int count = 1; count < crashCount && delay < m_policy.maximumBackoffMs; ++count) {
        delay = delay > m_policy.maximumBackoffMs / 2
            ? m_policy.maximumBackoffMs
            : std::min(delay * 2, m_policy.maximumBackoffMs);
    }
    return delay;
}

QString toString(LifecycleState value)
{
    switch (value) {
    case LifecycleState::Stopped:
        return QStringLiteral("stopped");
    case LifecycleState::Starting:
        return QStringLiteral("starting");
    case LifecycleState::Handshaking:
        return QStringLiteral("handshaking");
    case LifecycleState::Running:
        return QStringLiteral("running");
    case LifecycleState::Stopping:
        return QStringLiteral("stopping");
    case LifecycleState::Backoff:
        return QStringLiteral("backoff");
    case LifecycleState::Disabled:
        return QStringLiteral("disabled");
    }
    return {};
}

QString toString(LifecycleAction value)
{
    switch (value) {
    case LifecycleAction::None:
        return QStringLiteral("none");
    case LifecycleAction::LaunchProcess:
        return QStringLiteral("launch-process");
    case LifecycleAction::AwaitHandshake:
        return QStringLiteral("await-handshake");
    case LifecycleAction::TerminateProcess:
        return QStringLiteral("terminate-process");
    case LifecycleAction::ScheduleRestart:
        return QStringLiteral("schedule-restart");
    }
    return {};
}

} // namespace QindaQt::AppletHost
