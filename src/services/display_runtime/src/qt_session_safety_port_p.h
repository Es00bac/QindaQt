// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/platform/recovery_backoff/recovery_backoff.h>
#include <qindaqt/services/display_runtime/session_safety_port.h>

namespace QindaQt::DisplayRuntime
{

// Deterministic seam for the logind delay re-acquisition schedule (ADR-0379):
// tests inject manual timers; production uses the public factory.
[[nodiscard]] std::unique_ptr<SessionSafetyPort> makeQtSessionSafetyPort(
    const QDBusConnection &sessionConnection,
    const QDBusConnection &systemConnection,
    const Platform::Recovery::RecoveryTimerFactory &timers);

} // namespace QindaQt::DisplayRuntime
