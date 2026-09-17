// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/smart_lights_applet/smart_lights_applet_presentation.h"

#include "qindaqt/services/wiz_protocol/wiz_scenes.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>

namespace QindaQt::Shell::SmartLightsApplet
{
namespace
{

[[nodiscard]] QString translate(const char *text)
{
    return QCoreApplication::translate("QindaQt::Shell::SmartLightsApplet", text);
}

[[nodiscard]] QString colorHex(const Wiz::PilotState &pilot)
{
    if (!pilot.colorKnown) {
        return {};
    }
    if (pilot.red == 0 && pilot.green == 0 && pilot.blue == 0) {
        return {};
    }
    return QStringLiteral("#%1%2%3")
        .arg(pilot.red, 2, 16, QLatin1Char('0'))
        .arg(pilot.green, 2, 16, QLatin1Char('0'))
        .arg(pilot.blue, 2, 16, QLatin1Char('0'));
}

[[nodiscard]] QString sceneName(const Wiz::PilotState &pilot)
{
    if (!pilot.sceneKnown || pilot.sceneId == 0) {
        return {};
    }
    const auto scene = Wiz::sceneById(pilot.sceneId);
    return scene.has_value() ? scene->name : QString();
}

[[nodiscard]] QString statusLabel(const Wiz::Device &device)
{
    if (device.reachability == Wiz::Reachability::Unreachable) {
        return translate("Not responding");
    }
    if (!device.pilotKnown) {
        return translate("Checking…");
    }
    if (!device.pilot.on) {
        return translate("Off");
    }

    QStringList parts;
    switch (device.pilot.mode()) {
    case Wiz::LightMode::Scene: {
        const QString name = sceneName(device.pilot);
        parts.append(name.isEmpty() ? translate("Scene") : name);
        break;
    }
    case Wiz::LightMode::White:
        if (device.pilot.temperatureKnown && device.pilot.temperatureKelvin > 0) {
            parts.append(translate("%1 K").arg(device.pilot.temperatureKelvin));
        } else {
            parts.append(translate("White"));
        }
        break;
    case Wiz::LightMode::Color:
        parts.append(translate("Colour"));
        break;
    case Wiz::LightMode::Unknown:
        parts.append(translate("On"));
        break;
    }
    if (device.pilot.dimmingKnown) {
        parts.append(translate("%1%").arg(device.pilot.dimmingPercent));
    }
    if (device.reachability == Wiz::Reachability::Stale) {
        parts.append(translate("last seen a moment ago"));
    }
    return parts.join(translate(" · "));
}

[[nodiscard]] DeviceRow projectDevice(const Wiz::Device &device, const QString &rowId,
                                      const bool controlGranted)
{
    DeviceRow row;
    row.id = rowId;
    row.label = device.label;
    row.statusLabel = statusLabel(device);
    row.on = device.pilotKnown && device.pilot.on;
    row.reachable = device.reachability == Wiz::Reachability::Online
        || device.reachability == Wiz::Reachability::Stale;
    row.controllable = controlGranted && row.reachable;
    row.capabilitiesKnown = device.capabilitiesKnown;

    // AGENT-GUARD: a capability-dependent control is offered only once the
    // device has said what it can do. The client refuses anything but power
    // while capabilities are unknown, so advertising a slider here would
    // render an enabled control that cannot be dispatched.
    const bool proven = device.capabilitiesKnown;

    row.supportsDimming = proven && device.features.testFlag(Wiz::Feature::Dimming);
    row.minimumBrightnessPercent = device.dimming.minimumPercent;
    row.brightnessPercent = device.pilot.dimmingKnown
        ? static_cast<int>(device.pilot.dimmingPercent)
        : device.dimming.minimumPercent;

    row.supportsTemperature = proven
        && device.features.testFlag(Wiz::Feature::ColorTemperature)
        && device.temperature.isValid();
    row.minimumKelvin = device.temperature.minimumKelvin;
    row.maximumKelvin = device.temperature.maximumKelvin;
    // A light that is not in white mode has no meaningful temperature to show;
    // the control starts from the middle of its own range instead of zero.
    row.temperatureKelvin = device.pilot.temperatureKnown
            && device.pilot.temperatureKelvin > 0
        ? static_cast<int>(device.pilot.temperatureKelvin)
        : (row.minimumKelvin + row.maximumKelvin) / 2;

    row.supportsColor = proven && device.features.testFlag(Wiz::Feature::Color);
    row.colorHex = colorHex(device.pilot);

    row.supportsScenes = proven && device.features.testFlag(Wiz::Feature::Scenes);
    row.sceneId = device.pilot.sceneKnown ? static_cast<int>(device.pilot.sceneId) : 0;
    row.sceneName = sceneName(device.pilot);

    const auto activeScene = Wiz::sceneById(device.pilot.sceneId);
    row.supportsSpeed = proven && device.features.testFlag(Wiz::Feature::SceneSpeed);
    row.speedApplies = row.supportsSpeed && activeScene.has_value() && activeScene->dynamic;
    row.speedPercent =
        device.pilot.speedKnown ? static_cast<int>(device.pilot.speedPercent) : 100;

    row.signalKnown = device.pilot.signalKnown;
    row.signalDbm = device.pilot.signalDbm;

    row.accessibleName = device.label;
    row.accessibleDescription = row.statusLabel;
    return row;
}

[[nodiscard]] PresetRow projectPreset(const SmartLights::StoredPreset &preset,
                                      const QList<Wiz::Device> &devices,
                                      const bool controlGranted)
{
    PresetRow row;
    row.id = preset.id;
    row.name = preset.name;

    int present = 0;
    for (const SmartLights::PresetMember &member : preset.members) {
        for (const Wiz::Device &device : devices) {
            if (device.identity.mac != member.mac) {
                continue;
            }
            if (device.reachability == Wiz::Reachability::Online
                || device.reachability == Wiz::Reachability::Stale) {
                ++present;
            }
            break;
        }
    }
    row.applicable = controlGranted && present > 0;
    const int total = static_cast<int>(preset.members.size());
    row.summary = present == total
        ? translate("%1 lights").arg(total)
        : translate("%1 of %2 lights available").arg(present).arg(total);
    row.accessibleName = preset.name;
    row.accessibleDescription = row.applicable
        ? translate("Apply the %1 arrangement").arg(preset.name)
        : translate("No light from the %1 arrangement is responding").arg(preset.name);
    return row;
}

} // namespace

SmartLightsAppletModel projectSmartLightsApplet(
    const Wiz::Snapshot &snapshot, const QList<SmartLights::StoredPreset> &presets,
    const QHash<QString, QString> &rowIds, const bool readGranted,
    const bool controlGranted)
{
    SmartLightsAppletModel model;
    if (!readGranted) {
        model.phase = ServicePhase::Unavailable;
        model.summaryLabel = translate("Smart lights");
        model.accessibleName = translate("Smart lights are unavailable");
        model.accessibleDescription =
            translate("This desktop is not permitted to control smart lights.");
        model.diagnostic =
            translate("Smart light access has not been granted to the panel.");
        return model;
    }

    model.epoch = snapshot.epoch;
    model.revision = snapshot.revision;
    model.discovering = snapshot.discovering;
    model.controlGranted = controlGranted;
    model.diagnostic = snapshot.diagnostic;

    switch (snapshot.availability) {
    case Wiz::Availability::Starting:
        model.phase = ServicePhase::Loading;
        break;
    case Wiz::Availability::Ready:
    case Wiz::Availability::Degraded:
        model.phase = ServicePhase::Ready;
        break;
    case Wiz::Availability::Unavailable:
        model.phase = ServicePhase::Unavailable;
        break;
    }

    if (model.phase == ServicePhase::Ready) {
        for (const Wiz::Device &device : snapshot.devices) {
            const QString rowId = rowIds.value(device.identity.mac);
            if (rowId.isEmpty()) {
                continue;
            }
            const DeviceRow row = projectDevice(device, rowId, controlGranted);
            if (row.reachable) {
                ++model.reachableCount;
            }
            if (row.on && row.reachable) {
                ++model.onCount;
            }
            model.devices.append(row);
        }
        for (const SmartLights::StoredPreset &preset : presets) {
            model.presets.append(projectPreset(preset, snapshot.devices, controlGranted));
        }
    }

    const int total = static_cast<int>(model.devices.size());
    if (model.phase == ServicePhase::Unavailable) {
        model.summaryLabel = translate("Lights unavailable");
    } else if (model.phase == ServicePhase::Loading) {
        model.summaryLabel = translate("Looking for lights…");
    } else if (total == 0) {
        model.summaryLabel = snapshot.discovering ? translate("Searching…")
                                                  : translate("No lights found");
    } else if (model.onCount == 0) {
        model.summaryLabel = translate("All off");
    } else {
        model.summaryLabel = translate("%1 on").arg(model.onCount);
    }

    model.accessibleName = total == 0
        ? translate("Smart lights: %1").arg(model.summaryLabel)
        : translate("Smart lights: %1 of %2 on").arg(model.onCount).arg(total);
    model.accessibleDescription = total == 0
        ? translate("No smart lights have answered on this network yet.")
        : translate("Opens brightness, colour, scene, and preset controls for "
                    "the lights on this network.");
    return model;
}

} // namespace QindaQt::Shell::SmartLightsApplet
