// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_protocol/wiz_messages.h>
#include <qindaqt/services/wiz_protocol/wiz_types.h>

#include <optional>

namespace QindaQt::Wiz
{

// Everything the shell needs to know about what one luminaire can be asked to
// do, derived only from what the device itself reported.
struct CapabilityProfile {
    Features features;
    DimmingRange dimming;
    TemperatureRange temperature;
    // False until a model configuration has been seen. Callers must keep
    // capability-dependent controls unavailable while this is false rather
    // than assuming the defaults below are the device's real limits.
    bool complete = false;

    friend bool operator==(const CapabilityProfile &, const CapabilityProfile &) = default;
};

// Derives the profile from the module name and, when available, the device's
// model configuration.
//
// AGENT-NOTE: the module name is the vendor's own product encoding
// ("ESP25_SHRGB_01"): the middle token names the channel set, and it is the
// only evidence of colour capability that does not depend on the light's
// current state. The model configuration then narrows the numeric ranges.
[[nodiscard]] CapabilityProfile inferCapabilities(
    const QString &moduleName,
    const std::optional<ModelConfigPayload> &modelConfig);

} // namespace QindaQt::Wiz
