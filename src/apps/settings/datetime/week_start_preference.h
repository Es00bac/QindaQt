// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_datetime/system_time_service.h"

#include <QDBusConnection>

#include <memory>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
class QtSettingsTransport;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Apps::SettingsDateTime {

// Production WeekStartPreference (ADR-0211) over Settings1's
// `services.calendarWeekStart` -- the key the Calendar's month grid already
// reads, which until now had no editor anywhere.
//
// AGENT-GUARD: the Settings1 client is purpose-scoped to that one key, the
// same way CalendarPreferences scopes its own to the three the Calendar
// reads. A SettingsClient is constructed with the exact key list it wants;
// keeping this one at a single key is what stops the route from subscribing
// to -- or being disturbed by -- settings it has no business seeing. Do not
// widen the list to "while we are here".
class SettingsWeekStartPreference final : public WeekStartPreference {
  Q_OBJECT

public:
  explicit SettingsWeekStartPreference(QDBusConnection bus,
                                       QObject *parent = nullptr);
  ~SettingsWeekStartPreference() override;

  [[nodiscard]] QString weekStart() const override;
  [[nodiscard]] bool editable() const override;
  void setWeekStart(const QString &weekStart) override;

private:
  void applySnapshot();

  std::unique_ptr<QindaQt::Services::SettingsClient::QtSettingsTransport> m_transport;
  std::unique_ptr<QindaQt::Services::SettingsClient::SettingsClient> m_client;
  QString m_weekStart = QStringLiteral("locale");
  bool m_ready = false;
};

} // namespace QindaQt::Apps::SettingsDateTime
