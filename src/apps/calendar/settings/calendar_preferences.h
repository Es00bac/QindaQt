// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <memory>

namespace QindaQt::Services::SettingsClient {
class QtSettingsTransport;
class SettingsClient;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Apps::Calendar {

// Reads the three services.calendar* Settings1 keys for the Calendar app.
// AGENT-CONTRACT: when the session bus or Settings1 service is unavailable
// (offscreen probes unset DBUS_SESSION_BUS_ADDRESS), every getter returns the
// schema default. The degradation path must not qWarning — offscreen test
// rows run with QT_FATAL_WARNINGS=1 — so it logs once at qInfo level.
// Enum-ish values are validated against their allowed sets; a mismatched
// stored value falls back to the default rather than propagating.
class CalendarPreferences final : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString defaultView READ defaultView NOTIFY preferencesChanged)
  Q_PROPERTY(int weekStart READ weekStart NOTIFY preferencesChanged)
  Q_PROPERTY(QString defaultCalendarId READ defaultCalendarId NOTIFY
                 preferencesChanged)

public:
  static constexpr auto defaultViewValue = "month";
  static constexpr auto defaultCalendarIdValue = "personal";

  explicit CalendarPreferences(QObject *parent = nullptr);
  ~CalendarPreferences() override;

  // month | week | day
  [[nodiscard]] QString defaultView() const;
  // Qt::Monday … Qt::Sunday; "locale" resolves through QLocale.
  [[nodiscard]] int weekStart() const;
  [[nodiscard]] QString defaultCalendarId() const;

signals:
  void preferencesChanged();

private:
  void applySnapshot();

  std::unique_ptr<QindaQt::Services::SettingsClient::QtSettingsTransport>
      m_transport;
  std::unique_ptr<QindaQt::Services::SettingsClient::SettingsClient> m_client;
  QString m_defaultView;
  int m_weekStart = 1; // Qt::Monday
  QString m_defaultCalendarId;
};

} // namespace QindaQt::Apps::Calendar
