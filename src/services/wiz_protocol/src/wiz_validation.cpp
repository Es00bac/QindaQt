// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_protocol/wiz_validation.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"
#include "qindaqt/services/wiz_protocol/wiz_scenes.h"

#include <algorithm>

namespace QindaQt::Wiz
{
namespace
{

[[nodiscard]] ValidatedRequest refuse(const ValidationOutcome outcome,
                                      const QString &reasonCode)
{
    ValidatedRequest result;
    result.outcome = outcome;
    result.reasonCode = reasonCode;
    return result;
}

} // namespace

ValidatedRequest validateStateRequest(const Device &device, const StateRequest &request)
{
    if (request.isEmpty()) {
        return refuse(ValidationOutcome::Rejected, QStringLiteral("empty-request"));
    }
    if (!device.capabilitiesKnown) {
        // Power is the one intent a device proves it supports by answering.
        const bool powerOnly = request.setPower && !request.setDimming
            && !request.setTemperature && !request.setColor && !request.setScene
            && !request.setSpeed;
        if (!powerOnly) {
            return refuse(ValidationOutcome::Unsupported,
                          QStringLiteral("capabilities-unknown"));
        }
    }

    StateRequest sanitized;

    if (request.setPower) {
        sanitized.setPower = true;
        sanitized.on = request.on;
        if (!request.on) {
            // Switching off is sent alone, so the light keeps the colour it
            // will return to.
            ValidatedRequest accepted;
            accepted.outcome = ValidationOutcome::Accepted;
            accepted.request = sanitized;
            return accepted;
        }
    }

    // Exactly one colour lane survives: scene beats colour, colour beats white.
    if (request.setScene) {
        if (!device.features.testFlag(Feature::Scenes)) {
            return refuse(ValidationOutcome::Unsupported, QStringLiteral("no-scenes"));
        }
        if (!sceneSupported(request.sceneId, device.features)) {
            return refuse(ValidationOutcome::Unsupported,
                          QStringLiteral("scene-unsupported"));
        }
        sanitized.setScene = true;
        sanitized.sceneId = request.sceneId;
    } else if (request.setColor) {
        if (!device.features.testFlag(Feature::Color)) {
            return refuse(ValidationOutcome::Unsupported, QStringLiteral("no-color"));
        }
        sanitized.setColor = true;
        sanitized.red = request.red;
        sanitized.green = request.green;
        sanitized.blue = request.blue;
        sanitized.coolWhite = request.coolWhite;
        sanitized.warmWhite = request.warmWhite;
    } else if (request.setTemperature) {
        if (!device.features.testFlag(Feature::ColorTemperature)) {
            return refuse(ValidationOutcome::Unsupported,
                          QStringLiteral("no-color-temperature"));
        }
        if (!device.temperature.isValid()) {
            return refuse(ValidationOutcome::Unsupported,
                          QStringLiteral("temperature-range-unknown"));
        }
        sanitized.setTemperature = true;
        sanitized.temperatureKelvin = static_cast<quint16>(
            std::clamp(static_cast<int>(request.temperatureKelvin),
                       device.temperature.minimumKelvin,
                       device.temperature.maximumKelvin));
    }

    if (request.setDimming) {
        if (!device.features.testFlag(Feature::Dimming)) {
            return refuse(ValidationOutcome::Unsupported, QStringLiteral("no-dimming"));
        }
        sanitized.setDimming = true;
        sanitized.dimmingPercent = static_cast<quint8>(
            std::clamp(static_cast<int>(request.dimmingPercent),
                       device.dimming.minimumPercent, device.dimming.maximumPercent));
    }

    if (request.setSpeed) {
        if (!device.features.testFlag(Feature::SceneSpeed)) {
            return refuse(ValidationOutcome::Unsupported, QStringLiteral("no-scene-speed"));
        }
        // Speed only means something while a dynamic programme is playing. The
        // scene being selected in this same request counts; otherwise the one
        // the device last reported does.
        const quint16 sceneId = sanitized.setScene
            ? sanitized.sceneId
            : (device.pilot.sceneKnown ? device.pilot.sceneId : quint16{0});
        const auto scene = sceneById(sceneId);
        if (!scene.has_value() || !scene->dynamic) {
            return refuse(ValidationOutcome::Unsupported,
                          QStringLiteral("scene-not-dynamic"));
        }
        sanitized.setSpeed = true;
        // AGENT-NOTE (verified on firmware 1.38.0): a speed below the vendor
        // minimum is answered with "Invalid params" (-32602) rather than being
        // clamped, so the floor is enforced before the datagram is built.
        sanitized.speedPercent = static_cast<quint8>(
            std::clamp(static_cast<int>(request.speedPercent),
                       Limits::minimumSpeedPercent, Limits::maximumSpeedPercent));
    }

    if (sanitized.isEmpty()) {
        return refuse(ValidationOutcome::Rejected, QStringLiteral("nothing-to-apply"));
    }

    ValidatedRequest accepted;
    accepted.outcome = ValidationOutcome::Accepted;
    accepted.request = sanitized;
    return accepted;
}

} // namespace QindaQt::Wiz
