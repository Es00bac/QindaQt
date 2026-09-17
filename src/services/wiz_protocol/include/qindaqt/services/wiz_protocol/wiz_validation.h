// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_protocol/wiz_types.h>

namespace QindaQt::Wiz
{

enum class ValidationOutcome : quint32 {
    Accepted = 0,
    // The luminaire has no such channel. Nothing is sent.
    Unsupported = 1,
    // The request is malformed or empty. Nothing is sent.
    Rejected = 2,
};

struct ValidatedRequest {
    ValidationOutcome outcome = ValidationOutcome::Rejected;
    // The request as it will go on the wire: clamped to the device's own
    // ranges and collapsed to a single colour lane.
    StateRequest request;
    QString reasonCode;

    [[nodiscard]] bool accepted() const noexcept
    {
        return outcome == ValidationOutcome::Accepted;
    }

    friend bool operator==(const ValidatedRequest &, const ValidatedRequest &) = default;
};

// Admits a control intent against one device's proven capabilities.
//
// AGENT-CONTRACT: this is the only place that decides what may be sent to a
// luminaire. The client dispatches nothing that did not come back Accepted,
// and it sends exactly the returned request, so a bound that is missing here
// is a bound that does not exist anywhere.
//
// Three rules are not obvious from the vendor documentation and are enforced
// here because the firmware will not enforce them for us:
//   * Colour, white temperature, and scene are mutually exclusive lanes in one
//     setPilot. The strongest requested lane wins and the others are dropped.
//   * Turning a light off is sent alone. A device that is being switched off
//     must not also have its stored colour rewritten.
//   * A temperature outside the device's cctRange is clamped locally, because
//     the firmware clamps silently and would otherwise leave the projected
//     control disagreeing with the light.
[[nodiscard]] ValidatedRequest validateStateRequest(const Device &device,
                                                    const StateRequest &request);

} // namespace QindaQt::Wiz
