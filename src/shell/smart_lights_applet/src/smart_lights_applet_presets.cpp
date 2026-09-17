// SPDX-License-Identifier: GPL-3.0-or-later

#include "smart_lights_applet_controller.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"

#include <QtCore/QCoreApplication>

namespace QindaQt::Shell::SmartLightsApplet
{
namespace
{

[[nodiscard]] QString translate(const char *text)
{
    return QCoreApplication::translate("QindaQt::Shell::SmartLightsApplet", text);
}

} // namespace

std::optional<Wiz::StateRequest> SmartLightsAppletController::captureState(
    const Wiz::Device &device) const
{
    if (!device.pilotKnown) {
        return std::nullopt;
    }
    Wiz::StateRequest state;
    state.setPower = true;
    state.on = device.pilot.on;
    if (!device.pilot.on) {
        // A light that is off contributes exactly that to an arrangement.
        return state;
    }

    switch (device.pilot.mode()) {
    case Wiz::LightMode::Scene:
        state.setScene = true;
        state.sceneId = device.pilot.sceneId;
        if (device.pilot.speedKnown
            && device.features.testFlag(Wiz::Feature::SceneSpeed)) {
            state.setSpeed = true;
            state.speedPercent = device.pilot.speedPercent;
        }
        break;
    case Wiz::LightMode::Color:
        state.setColor = true;
        state.red = device.pilot.red;
        state.green = device.pilot.green;
        state.blue = device.pilot.blue;
        state.coolWhite = device.pilot.coolWhite;
        state.warmWhite = device.pilot.warmWhite;
        break;
    case Wiz::LightMode::White:
        if (device.pilot.temperatureKnown && device.pilot.temperatureKelvin > 0) {
            state.setTemperature = true;
            state.temperatureKelvin = device.pilot.temperatureKelvin;
        }
        break;
    case Wiz::LightMode::Unknown:
        break;
    }

    if (device.pilot.dimmingKnown) {
        state.setDimming = true;
        state.dimmingPercent = device.pilot.dimmingPercent;
    }
    return state;
}

bool SmartLightsAppletController::savePreset(const QString &name)
{
    const QString trimmed = name.trimmed().left(Wiz::Limits::maximumLabelLength);
    if (trimmed.isEmpty()) {
        publishFeedback(translate("Give the arrangement a name first."));
        return false;
    }
    if (m_client == nullptr || !m_readGranted) {
        return false;
    }
    if (m_configuration.presets.size() >= Wiz::Limits::maximumPresets) {
        publishFeedback(translate("There is no room for another saved arrangement."));
        return false;
    }

    SmartLights::StoredPreset preset;
    preset.name = trimmed;
    preset.id = SmartLights::makePresetId(trimmed, m_configuration.presets);

    const Wiz::Snapshot snapshot = m_client->snapshot();
    for (const Wiz::Device &device : snapshot.devices) {
        if (device.reachability == Wiz::Reachability::Unreachable
            || device.reachability == Wiz::Reachability::Unknown) {
            continue;
        }
        const auto state = captureState(device);
        if (!state.has_value()) {
            continue;
        }
        SmartLights::PresetMember member;
        member.mac = device.identity.mac;
        member.state = *state;
        preset.members.append(member);
    }

    if (preset.members.isEmpty()) {
        publishFeedback(translate("No light is responding, so there is nothing to save."));
        return false;
    }

    m_configuration.presets.append(preset);
    persistConfiguration();
    publishFeedback(translate("Saved “%1”.").arg(trimmed));
    reproject();
    Q_EMIT stateChanged();
    return true;
}

bool SmartLightsAppletController::applyPreset(const QString &presetId)
{
    for (const SmartLights::StoredPreset &preset : m_configuration.presets) {
        if (preset.id != presetId) {
            continue;
        }
        bool dispatched = false;
        for (const SmartLights::PresetMember &member : preset.members) {
            Wiz::OperationRequest request;
            request.kind = Wiz::OperationKind::ApplyPreset;
            request.targetMac = member.mac;
            request.state = member.state;
            // A light that has since been unplugged is skipped silently; the
            // rest of the arrangement still applies.
            if (dispatch(request)) {
                dispatched = true;
            }
        }
        if (!dispatched) {
            publishFeedback(
                translate("No light from “%1” is responding.").arg(preset.name));
        } else {
            clearFeedback();
        }
        return dispatched;
    }
    return false;
}

bool SmartLightsAppletController::deletePreset(const QString &presetId)
{
    for (qsizetype index = 0; index < m_configuration.presets.size(); ++index) {
        if (m_configuration.presets.at(index).id != presetId) {
            continue;
        }
        m_configuration.presets.removeAt(index);
        persistConfiguration();
        reproject();
        Q_EMIT stateChanged();
        return true;
    }
    return false;
}

bool SmartLightsAppletController::requestRename(const QString &rowId,
                                                const QString &label)
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty() || m_client == nullptr) {
        return false;
    }
    const QString trimmed = label.trimmed().left(Wiz::Limits::maximumLabelLength);

    bool found = false;
    for (SmartLights::StoredDevice &stored : m_configuration.devices) {
        if (stored.mac != mac) {
            continue;
        }
        stored.label = trimmed;
        found = true;
        break;
    }
    if (!found) {
        SmartLights::StoredDevice stored;
        stored.mac = mac;
        stored.label = trimmed;
        m_configuration.devices.append(stored);
    }

    m_client->applyStoredLabel(mac, trimmed);
    persistConfiguration();
    reproject();
    return true;
}

bool SmartLightsAppletController::requestForget(const QString &rowId)
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty() || m_client == nullptr) {
        return false;
    }

    for (qsizetype index = m_configuration.devices.size() - 1; index >= 0; --index) {
        if (m_configuration.devices.at(index).mac == mac) {
            m_configuration.devices.removeAt(index);
        }
    }
    // A forgotten light also leaves every arrangement it was part of; an
    // arrangement with no members left goes with it.
    for (qsizetype index = m_configuration.presets.size() - 1; index >= 0; --index) {
        SmartLights::StoredPreset &preset = m_configuration.presets[index];
        for (qsizetype member = preset.members.size() - 1; member >= 0; --member) {
            if (preset.members.at(member).mac == mac) {
                preset.members.removeAt(member);
            }
        }
        if (preset.members.isEmpty()) {
            m_configuration.presets.removeAt(index);
        }
    }

    m_client->forgetDevice(mac);
    // The token is retired with the device: a light that is discovered again
    // gets a fresh one, so a stale row cannot address a different luminaire.
    m_rowIds.remove(mac);
    m_rowMacs.remove(rowId);
    persistConfiguration();
    reproject();
    Q_EMIT stateChanged();
    return true;
}

} // namespace QindaQt::Shell::SmartLightsApplet
