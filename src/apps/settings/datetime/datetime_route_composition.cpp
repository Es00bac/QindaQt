// SPDX-License-Identifier: GPL-3.0-or-later
#include "datetime_route_composition.h"

#include "qindaqt/apps/settings_datetime/datetime_settings_model.h"
#include "systemd_time_service.h"
#include "week_start_preference.h"

#include <QDBusConnection>

namespace QindaQt::Apps::SettingsDateTime {

class DateTimeRouteComposition::Private final {
public:
  Private()
      : model(std::make_unique<SystemdTimeService>(QDBusConnection::systemBus()),
              std::make_unique<SettingsWeekStartPreference>(
                  QDBusConnection::sessionBus())) {}

  // AGENT-CONTRACT: two buses on purpose. timedate1 and locale1 are
  // system-wide services on the system bus; Settings1 is the user's own
  // session service. Nothing here crosses them.
  DateTimeSettingsModel model;
};

DateTimeRouteComposition::DateTimeRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

DateTimeRouteComposition::~DateTimeRouteComposition() = default;

QObject *DateTimeRouteComposition::model() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsDateTime
