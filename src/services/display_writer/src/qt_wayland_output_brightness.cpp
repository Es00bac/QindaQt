// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qt_wayland_output_objects_p.h"

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

namespace QindaQt::DisplayWriter::Private
{

QList<OutputDeviceState> brightnessDeviceStates(
    const std::vector<std::unique_ptr<OutputDevice>> &devices,
    const bool managementCanSetBrightness)
{
    QList<OutputDeviceState> states;
    states.reserve(static_cast<qsizetype>(devices.size()));
    for (const auto &device : devices) {
        const std::optional<quint32> brightness = device->brightness();
        const bool observed = brightness.has_value() && *brightness <= Display::kMaxBrightness;
        states.push_back({.connectorName = device->connectorName(),
                          .uuid = device->uuid(),
                          .enabled = device->enabled(),
                          .brightnessCapable =
                              managementCanSetBrightness && device->brightnessCapable(),
                          .brightnessObserved = observed,
                          .brightness = observed ? *brightness : 0U});
    }
    return states;
}

BrightnessTarget brightnessTarget(const std::vector<std::unique_ptr<OutputDevice>> &devices,
                                  const BrightnessConfiguration &configuration)
{
    if (configuration.requestId == 0 || configuration.connectorName.isEmpty()
        || configuration.uuid.isEmpty()
        || !Display::isBoundedText(configuration.connectorName,
                                   Display::kMaxConnectorNameUtf8Bytes)
        || !Display::isBoundedText(configuration.uuid, Display::kMaxRuntimeUuidUtf8Bytes)
        || configuration.brightness > Display::kMaxBrightness) {
        return {.device = nullptr, .refusal = SubmitStatus::Malformed};
    }
    OutputDevice *target = nullptr;
    for (const auto &device : devices) {
        if (!device->ready()) {
            return {.device = nullptr, .refusal = SubmitStatus::Malformed};
        }
        if (device->connectorName() == configuration.connectorName) {
            if (target != nullptr) {
                return {.device = nullptr, .refusal = SubmitStatus::Malformed};
            }
            target = device.get();
        }
    }
    // AGENT-GUARD: KWin 6.6.6 stores set_brightness for any live device and
    // acknowledges outputs whose backend ignores it. The exact UUID, enabled
    // state, and advertised capability are this adapter's refusal boundary;
    // never submit and rely on a compositor failure.
    if (target == nullptr || target->uuid() != configuration.uuid || !target->enabled()
        || !target->brightnessCapable()) {
        return {.device = nullptr, .refusal = SubmitStatus::Unsupported};
    }
    return {.device = target, .refusal = SubmitStatus::Accepted};
}

} // namespace QindaQt::DisplayWriter::Private
