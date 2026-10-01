// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/session/idle_policy/display_off_stage.h>

#include <functional>
#include <memory>

namespace QindaQt::Session::IdlePolicy {

// Public production boundary for ordinary compositor display power. The
// supplied descriptor must already be admitted and connected; the lineage
// predicate must remain valid for the port lifetime. The implementation owns
// each returned descriptor, invokes the FD supplier only during start, and
// rechecks lineage before ordinary requests. Final restoration after revocation
// uses only the retained peer FD and never opens a replacement connection.
// Construct, call and destroy the port on one Qt event-loop thread.
class AttachedDisplayPowerPort : public DisplayPowerPort {
public:
    using DisplayPowerPort::DisplayPowerPort;
    using OpenAdmittedFd = std::function<int()>;
    using LineageLive = std::function<bool()>;
    ~AttachedDisplayPowerPort() override = default;

    // Returns false with an explanation if the initial attachment cannot be
    // established. Later attachment loss is reported through availability.
    virtual bool start(OpenAdmittedFd openAdmittedFd, LineageLive lineageLive,
                       QString *error = nullptr) = 0;
};

// Creates the platform implementation without exposing its private protocol
// type. The returned object is caller-owned; callback captures must outlive it.
[[nodiscard]] std::unique_ptr<AttachedDisplayPowerPort>
makeAttachedDisplayPowerPort();

} // namespace QindaQt::Session::IdlePolicy
