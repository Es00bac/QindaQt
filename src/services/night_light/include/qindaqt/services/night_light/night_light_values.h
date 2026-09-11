// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QTime>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::Services::NightLight {

// AGENT-CONTRACT: These bounds mirror what the pinned KWin nightlight plugin
// accepts. The installed /usr/share/config.kcfg/nightlightsettings.kcfg
// declares no min/max, so QindaQt's own bounded profile below is the contract
// (ADR-0136); widening it requires a kcfg change upstream first.
inline constexpr int kTemperatureFloorKelvin = 1000;
inline constexpr int kTemperatureCeilingKelvin = 6500;
inline constexpr int kTemperatureStepKelvin = 100;
// 6500 K is KWin's neutral point: an inhibited or disabled night light
// reports it on the bus.
inline constexpr int kNeutralTemperatureKelvin = kTemperatureCeilingKelvin;
inline constexpr int kDefaultDayTemperatureKelvin = 6500;
inline constexpr int kDefaultNightTemperatureKelvin = 4500;

// knighttimerc Times.TransitionDuration is stored in seconds (upstream
// knighttime multiplies legacy minutes by 60 when migrating). The daemon
// default is 1800 (30 minutes).
inline constexpr int kMinTransitionSeconds = 60;
inline constexpr int kMaxTransitionSeconds = 7200;
inline constexpr int kDefaultTransitionSeconds = 1800;

// AGENT-CONTRACT: These enumerators and their persisted forms are fixed by
// KWin's nightlightsettings.kcfg (<choices name="KWin::NightLightMode">:
// Constant=0, DarkLight=1 — the D-Bus XML's "0 automatic .. 3 constant" doc is
// outdated) and by knighttimerc's kcfg (Source: Location/Times). Both persist
// as choice-name strings; never write the integer forms to config.
enum class Mode {
    Constant = 0,
    DarkLight = 1,
};

enum class ScheduleSource {
    Location,
    Times,
};

struct OutputSettings {
    bool active = false;
    Mode mode = Mode::DarkLight;
    int dayTemperatureKelvin = kDefaultDayTemperatureKelvin;
    int nightTemperatureKelvin = kDefaultNightTemperatureKelvin;

    friend bool operator==(const OutputSettings &,
                           const OutputSettings &) = default;
};

struct ScheduleSettings {
    ScheduleSource source = ScheduleSource::Location;
    bool automaticLocation = true;
    double latitudeDegrees = 0.0;
    double longitudeDegrees = 0.0;
    QTime sunriseStart = QTime(6, 0, 0);
    QTime sunsetStart = QTime(18, 0, 0);
    int transitionSeconds = kDefaultTransitionSeconds;

    friend bool operator==(const ScheduleSettings &,
                           const ScheduleSettings &) = default;
};

struct NightLightSettings {
    OutputSettings output;
    ScheduleSettings schedule;

    friend bool operator==(const NightLightSettings &,
                           const NightLightSettings &) = default;
};

// Whole kelvin value inside [floor, ceiling] and on the step grid.
constexpr bool isValidTemperature(int kelvin)
{
    return kelvin >= kTemperatureFloorKelvin
           && kelvin <= kTemperatureCeilingKelvin
           && kelvin % kTemperatureStepKelvin == 0;
}

constexpr int clampTemperature(int kelvin)
{
    if (kelvin < kTemperatureFloorKelvin) {
        return kTemperatureFloorKelvin;
    }
    if (kelvin > kTemperatureCeilingKelvin) {
        return kTemperatureCeilingKelvin;
    }
    return kelvin;
}

// Clamp into range and snap to the nearest 100 K step.
int snapTemperature(int kelvin);

bool isValidLatitude(double degrees);
bool isValidLongitude(double degrees);
bool isValidTransitionSeconds(int seconds);

// Both times must be valid and strictly ordered (sunrise before sunset),
// matching KDarkLightSchedule::forecast's day-cycle requirement.
bool isValidScheduleTimes(QTime sunriseStart, QTime sunsetStart);

bool isValidOutput(const OutputSettings &output);
bool isValidSchedule(const ScheduleSettings &schedule);

// KConfigXT-compatible token mapping. Unknown tokens return nullopt and must
// fail closed; callers never guess a mode from a foreign token.
std::optional<Mode> modeFromConfigToken(const QString &token);
QString modeToConfigToken(Mode mode);
std::optional<ScheduleSource> sourceFromConfigToken(const QString &token);
QString sourceToConfigToken(ScheduleSource source);

// The KWin bus property `mode` carries the kcfg choice index (0 Constant,
// 1 DarkLight). Any other value is hostile.
std::optional<Mode> modeFromBusValue(quint32 value);

} // namespace QindaQt::Services::NightLight
