// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_schedule.h>

#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <vector>

namespace QindaQt::Services::NightLight {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kRadians = kPi / 180.0;
constexpr double kDegrees = 180.0 / kPi;

enum class Event { Sunrise, Sunset };
struct EventTime { QDateTime at; Event event; };

std::optional<QDateTime> solarEvent(const QDate &date, double latitude,
                                    double longitude, Event event,
                                    const QTimeZone &zone)
{
    const double n = date.dayOfYear();
    const double longitudeHour = longitude / 15.0;
    const double approximate = n + ((event == Event::Sunrise ? 6.0 : 18.0)
                                      - longitudeHour) / 24.0;
    const double meanAnomaly = 0.9856 * approximate - 3.289;
    double trueLongitude = meanAnomaly + 1.916 * std::sin(meanAnomaly * kRadians)
                           + 0.020 * std::sin(2.0 * meanAnomaly * kRadians) + 282.634;
    trueLongitude = std::fmod(trueLongitude + 360.0, 360.0);
    double rightAscension = std::atan(0.91764 * std::tan(trueLongitude * kRadians))
                            * kDegrees;
    rightAscension = std::fmod(rightAscension + 360.0, 360.0);
    rightAscension += std::floor(trueLongitude / 90.0) * 90.0
                      - std::floor(rightAscension / 90.0) * 90.0;
    rightAscension /= 15.0;
    const double sinDeclination = 0.39782 * std::sin(trueLongitude * kRadians);
    const double cosDeclination = std::cos(std::asin(sinDeclination));
    const double denominator = cosDeclination * std::cos(latitude * kRadians);
    if (std::abs(denominator) < 1e-12) return std::nullopt;
    const double cosHour = (std::cos(90.833 * kRadians)
        - sinDeclination * std::sin(latitude * kRadians)) / denominator;
    if (cosHour < -1.0 || cosHour > 1.0) return std::nullopt;
    const double hour = (event == Event::Sunrise ? 360.0 - std::acos(cosHour) * kDegrees
                                                  : std::acos(cosHour) * kDegrees) / 15.0;
    const double localMean = hour + rightAscension - 0.06571 * approximate - 6.622;
    double utcHours = std::fmod(localMean - longitudeHour + 24.0, 24.0);
    const int wholeSeconds = qRound(utcHours * 3600.0);
    QDateTime utc(date, QTime(0, 0), QTimeZone::UTC);
    utc = utc.addSecs(wholeSeconds);
    const QDateTime local = utc.toTimeZone(zone);
    return local.isValid() ? std::optional<QDateTime>(local) : std::nullopt;
}

std::optional<QDateTime> eventAt(const QDate &date, QTime time,
                                 std::optional<QPair<double, double>> location,
                                 Event event, const QTimeZone &zone)
{
    if (location) {
        return solarEvent(date, location->first, location->second, event, zone);
    }
    QDateTime local(date, time, zone);
    return local.isValid() ? std::optional<QDateTime>(local) : std::nullopt;
}
} // namespace

ScheduleFrame calculateSchedule(const NightLightSettings &settings,
                                const QDateTime &now,
                                std::optional<QPair<double, double>> automaticLocation)
{
    ScheduleFrame result;
    if (!now.isValid() || !settings.output.active
        || settings.output.mode == Mode::Constant) {
        result.available = now.isValid();
        result.daylight = true;
        return result;
    }
    if (!isValidOutput(settings.output) || !isValidSchedule(settings.schedule)) {
        result.diagnostic = QStringLiteral("night-light preferences failed validation");
        return result;
    }
    std::optional<QPair<double, double>> location;
    if (settings.schedule.source == ScheduleSource::Location) {
        if (settings.schedule.automaticLocation) {
            location = automaticLocation;
            if (!location) {
                result.diagnostic = QStringLiteral("automatic location is unavailable");
                return result;
            }
        } else {
            location = QPair{settings.schedule.latitudeDegrees,
                             settings.schedule.longitudeDegrees};
        }
        if (!isValidLatitude(location->first) || !isValidLongitude(location->second)) {
            result.diagnostic = QStringLiteral("location is outside the supported bounds");
            return result;
        }
    }
    const QTimeZone zone = now.timeZone();
    if (!zone.isValid()) {
        result.diagnostic = QStringLiteral("local time zone is unavailable");
        return result;
    }
    std::vector<EventTime> events;
    for (int offset = -2; offset <= 2; ++offset) {
        const QDate date = now.date().addDays(offset);
        const auto sunrise = eventAt(date, settings.schedule.sunriseStart, location,
                                     Event::Sunrise, zone);
        const auto sunset = eventAt(date, settings.schedule.sunsetStart, location,
                                    Event::Sunset, zone);
        if (!sunrise || !sunset || *sunrise >= *sunset) {
            result.diagnostic = location
                ? QStringLiteral("solar schedule is unavailable at this location")
                : QStringLiteral("fixed schedule times are invalid for local time");
            return result;
        }
        events.push_back({*sunrise, Event::Sunrise});
        events.push_back({*sunset, Event::Sunset});
    }
    std::sort(events.begin(), events.end(), [](const auto &a, const auto &b) {
        return a.at < b.at;
    });
    const auto next = std::find_if(events.begin(), events.end(),
                                   [&now](const auto &item) { return item.at > now; });
    if (next == events.end() || next == events.begin()) {
        result.diagnostic = QStringLiteral("schedule window is incomplete");
        return result;
    }
    const auto previous = std::prev(next);
    auto transition = [&settings](const EventTime &item) {
        return ScheduleTransition{item.at,
            item.at.addSecs(settings.schedule.transitionSeconds)};
    };
    result.previous = transition(*previous);
    result.next = transition(*next);
    result.daylight = previous->event == Event::Sunrise;
    result.available = true;
    return result;
}

} // namespace QindaQt::Services::NightLight
