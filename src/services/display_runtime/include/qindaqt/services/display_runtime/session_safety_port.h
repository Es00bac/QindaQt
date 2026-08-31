// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_transaction/transaction_types.h>

#include <QtCore/QString>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::DisplayRuntime
{

enum class SessionSafetyStartStatus {
    Started,
    AlreadyStarted,
    InvalidConnection,
    InvalidCompositorProcess,
    LockMonitorFailed,
    LogindRegistrationFailed,
};

class SessionSafetyObserver
{
public:
    virtual ~SessionSafetyObserver() = default;

    // ready is emitted after an exact-owner logind delay descriptor is held.
    // Authenticated lock state may still be Unknown, so callers must consult
    // currentSafety() rather than treating readiness as Safe.
    virtual void sessionSafetyReady() = 0;
    virtual void sessionSafetyChanged(
        DisplayTransaction::SafetyState safety) = 0;
    virtual void sessionPreparingForSleep() = 0;
    virtual void sessionAuthorityLost(const QString &reasonCode) = 0;
};

class SessionSafetyPort
{
public:
    virtual ~SessionSafetyPort() = default;

    // The observer is borrowed on the constructing Qt thread and must outlive
    // a started port. expectedCompositorProcessId is independently derived by
    // D4 from the exact Wayland socket peer. stop() suppresses later callbacks.
    virtual void setObserver(SessionSafetyObserver *observer) = 0;
    [[nodiscard]] virtual SessionSafetyStartStatus start(
        qint64 expectedCompositorProcessId) = 0;
    virtual void stop() = 0;
    virtual void releaseSleepDelay() = 0;
    [[nodiscard]] virtual bool delayHeld() const noexcept = 0;
    [[nodiscard]] virtual DisplayTransaction::SafetyState currentSafety()
        const noexcept = 0;
};

// Owns only Qt D-Bus transport, the accepted authenticated lock monitor, and
// the exact-owner logind sleep-delay descriptor. Both named connections must
// remain registered for the port lifetime.
[[nodiscard]] std::unique_ptr<SessionSafetyPort> makeQtSessionSafetyPort(
    const QDBusConnection &sessionConnection,
    const QDBusConnection &systemConnection);

} // namespace QindaQt::DisplayRuntime
