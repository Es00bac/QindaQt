// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/night_light/night_light_values.h>

#include <cmath>

namespace QindaQt::Services::NightLight {

namespace {
constexpr double kLatitudeBound = 90.0;
constexpr double kLongitudeBound = 180.0;
} // namespace

int snapTemperature(int kelvin)
{
    const int clamped = clampTemperature(kelvin);
    const int remainder = clamped % kTemperatureStepKelvin;
    if (remainder == 0) {
        return clamped;
    }
    const int base = clamped - remainder;
    // Snap to the nearest step; ties go up, but never past the ceiling.
    if (remainder >= kTemperatureStepKelvin / 2) {
        return base + kTemperatureStepKelvin > kTemperatureCeilingKelvin
                   ? kTemperatureCeilingKelvin
                   : base + kTemperatureStepKelvin;
    }
    return base < kTemperatureFloorKelvin ? kTemperatureFloorKelvin : base;
}

bool isValidLatitude(double degrees)
{
    return std::isfinite(degrees) && degrees >= -kLatitudeBound
           && degrees <= kLatitudeBound;
}

bool isValidLongitude(double degrees)
{
    return std::isfinite(degrees) && degrees >= -kLongitudeBound
           && degrees <= kLongitudeBound;
}

bool isValidTransitionSeconds(int seconds)
{
    return seconds >= kMinTransitionSeconds && seconds <= kMaxTransitionSeconds;
}

bool isValidScheduleTimes(QTime sunriseStart, QTime sunsetStart)
{
    return sunriseStart.isValid() && sunsetStart.isValid()
           && sunriseStart < sunsetStart;
}

bool isValidOutput(const OutputSettings &output)
{
    return isValidTemperature(output.dayTemperatureKelvin)
           && isValidTemperature(output.nightTemperatureKelvin);
}

bool isValidSchedule(const ScheduleSettings &schedule)
{
    switch (schedule.source) {
    case ScheduleSource::Location:
        return isValidLatitude(schedule.latitudeDegrees)
               && isValidLongitude(schedule.longitudeDegrees)
               && isValidTransitionSeconds(schedule.transitionSeconds);
    case ScheduleSource::Times:
        return isValidScheduleTimes(schedule.sunriseStart, schedule.sunsetStart)
               && isValidTransitionSeconds(schedule.transitionSeconds);
    }
    return false;
}

std::optional<Mode> modeFromConfigToken(const QString &token)
{
    if (token == QLatin1String("Constant")) {
        return Mode::Constant;
    }
    if (token == QLatin1String("DarkLight")) {
        return Mode::DarkLight;
    }
    return std::nullopt;
}

QString modeToConfigToken(Mode mode)
{
    switch (mode) {
    case Mode::Constant:
        return QStringLiteral("Constant");
    case Mode::DarkLight:
        return QStringLiteral("DarkLight");
    }
    return QString();
}

std::optional<ScheduleSource> sourceFromConfigToken(const QString &token)
{
    if (token == QLatin1String("Location")) {
        return ScheduleSource::Location;
    }
    if (token == QLatin1String("Times")) {
        return ScheduleSource::Times;
    }
    return std::nullopt;
}

QString sourceToConfigToken(ScheduleSource source)
{
    switch (source) {
    case ScheduleSource::Location:
        return QStringLiteral("Location");
    case ScheduleSource::Times:
        return QStringLiteral("Times");
    }
    return QString();
}

std::optional<Mode> modeFromBusValue(quint32 value)
{
    switch (value) {
    case 0:
        return Mode::Constant;
    case 1:
        return Mode::DarkLight;
    }
    return std::nullopt;
}

} // namespace QindaQt::Services::NightLight
