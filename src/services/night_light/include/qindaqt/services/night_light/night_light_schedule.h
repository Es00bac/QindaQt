// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/night_light/night_light_values.h>

#include <QDateTime>
#include <optional>

namespace QindaQt::Services::NightLight {

struct ScheduleTransition final {
    QDateTime start;
    QDateTime end;
    friend bool operator==(const ScheduleTransition &, const ScheduleTransition &) = default;
};

struct ScheduleFrame final {
    bool available = false;
    bool daylight = false;
    ScheduleTransition previous;
    ScheduleTransition next;
    QString diagnostic;
};

// Pure schedule calculation. Automatic location is an injected optional fix;
// an absent fix or a polar day/night produces unavailable truth rather than a
// guessed schedule. `now` must carry a time-zone-aware timestamp.
[[nodiscard]] ScheduleFrame calculateSchedule(
    const NightLightSettings &settings, const QDateTime &now,
    std::optional<QPair<double, double>> automaticLocation = std::nullopt);

} // namespace QindaQt::Services::NightLight
