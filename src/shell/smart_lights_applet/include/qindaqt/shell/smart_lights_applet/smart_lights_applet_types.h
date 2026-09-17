// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_protocol/wiz_types.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Shell::SmartLightsApplet
{

enum class ServicePhase {
    Loading,
    Ready,
    Unavailable,
};

// One luminaire as the panel presents it. Every control-shaped member is a
// bounded value plus the permission to use it, so QML never has to decide
// whether something may be dispatched.
//
// AGENT-GUARD: `id` is a session-scoped opaque token, never the device MAC.
// The controller owns the mapping. Nothing downstream of it may learn a
// hardware address.
struct DeviceRow {
    QString id;
    QString label;
    QString statusLabel;
    bool on = false;
    bool reachable = false;
    // The service is available, the capability is granted, and the device is
    // answering. False makes every control in this row inert.
    bool controllable = false;
    bool capabilitiesKnown = false;

    bool supportsDimming = false;
    int brightnessPercent = 0;
    int minimumBrightnessPercent = 10;

    bool supportsTemperature = false;
    int temperatureKelvin = 0;
    int minimumKelvin = 0;
    int maximumKelvin = 0;

    bool supportsColor = false;
    // "#rrggbb" for the current colour, empty when the device is not in a
    // colour mode.
    QString colorHex;

    bool supportsScenes = false;
    int sceneId = 0;
    QString sceneName;

    bool supportsSpeed = false;
    int speedPercent = 0;
    // Playback speed only means something while a dynamic programme runs.
    bool speedApplies = false;

    bool signalKnown = false;
    int signalDbm = 0;

    QString accessibleName;
    QString accessibleDescription;

    friend bool operator==(const DeviceRow &, const DeviceRow &) = default;
};

// A stored arrangement as the panel presents it.
struct PresetRow {
    QString id;
    QString name;
    QString summary;
    // At least one member device is present and controllable right now.
    bool applicable = false;
    QString accessibleName;
    QString accessibleDescription;

    friend bool operator==(const PresetRow &, const PresetRow &) = default;
};

struct SmartLightsAppletModel {
    ServicePhase phase = ServicePhase::Unavailable;
    QString diagnostic;
    quint64 epoch = 0;
    quint64 revision = 0;
    bool discovering = false;
    // Any control at all may be dispatched: the capability is granted and the
    // service is answering.
    bool controlGranted = false;
    QString summaryLabel;
    QString accessibleName;
    QString accessibleDescription;
    int onCount = 0;
    int reachableCount = 0;
    QList<DeviceRow> devices;
    QList<PresetRow> presets;

    friend bool operator==(const SmartLightsAppletModel &,
                           const SmartLightsAppletModel &) = default;
};

} // namespace QindaQt::Shell::SmartLightsApplet
