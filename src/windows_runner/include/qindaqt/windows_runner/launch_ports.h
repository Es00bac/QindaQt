// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/windows_runner/launch_types.h>

#include <memory>
#include <optional>

namespace QindaQt::WindowsRunner {

class WindowsRegistrationPort {
public:
    virtual ~WindowsRegistrationPort() = default;
    // Read-only, bounded copied lookup on the runner's thread. Missing or
    // unavailable provenance returns nullopt; exceptions are not permitted.
    [[nodiscard]] virtual std::optional<RegisteredWindowsApplication>
        resolve(const QString &registrationId) = 0;
};

class WindowsPayloadLease {
public:
    virtual ~WindowsPayloadLease() = default;
    [[nodiscard]] virtual const QVector<PayloadPin> &verifiedPayloads() const = 0;
    // AGENT-CONTRACT: the producer holds stable payload handles, verifies exact
    // package ownership/size/SHA-256 and materialization receipt, and detects
    // replacement. A path existence check or self-declared digest is inadequate.
    [[nodiscard]] virtual bool stillCurrent() const = 0;
};

class WindowsPayloadPort {
public:
    virtual ~WindowsPayloadPort() = default;
    // May perform bounded filesystem work; the caller invokes this coordinator
    // off a GUI thread. Failure returns null, preserving profile data intact.
    // A lease owns its handles through dispatch; the runner never copies them.
    [[nodiscard]] virtual std::unique_ptr<WindowsPayloadLease>
        acquire(const RegisteredWindowsApplication &application) = 0;
};

enum class LaunchDispatch { Accepted, Refused, OutcomeUnknown };

class WindowsLaunchPort {
public:
    virtual ~WindowsLaunchPort() = default;
    // Submit one literal program/argv with the plan's complete environment.
    // No shell interpolation, inherited overlay, downloader or retry. Consume
    // the lease's checked handles for dispatch, never reopen untrusted paths.
    // ProtonUmu must use the exact leased Steam Runtime/Proton tuple with runtime
    // downloads disabled. Until that adapter exists it must return Refused.
    // At dispatch, transactionally recheck current registration/profile authority
    // with their owner and hold that authority through submission. The core
    // copied recheck alone cannot close the final update/removal race.
    // Refused means no spawn or operational side effect; any uncertainty after
    // submission is OutcomeUnknown, never Refused and never a reason to replay.
    // Accepted means startup submitted, not Running or a qualified window.
    [[nodiscard]] virtual LaunchDispatch submit(const WindowsLaunchPlan &plan,
                                                WindowsPayloadLease &lease) = 0;
};

} // namespace QindaQt::WindowsRunner
