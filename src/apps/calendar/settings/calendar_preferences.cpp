// SPDX-License-Identifier: GPL-3.0-or-later
#include "calendar_preferences.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QDBusConnection>
#include <QLocale>
#include <QVariant>

namespace QindaQt::Apps::Calendar {
namespace {

constexpr auto keyDefaultView = "services.calendarDefaultView";
constexpr auto keyWeekStart = "services.calendarWeekStart";
constexpr auto keyDefaultCalendar = "services.calendarDefaultCalendar";

[[nodiscard]] QString normalizeView(const QString &value) {
  if (value == QLatin1String("month") || value == QLatin1String("week") ||
      value == QLatin1String("day")) {
    return value;
  }
  return QLatin1StringView(CalendarPreferences::defaultViewValue);
}

[[nodiscard]] int normalizeWeekStart(const QString &value) {
  if (value == QLatin1String("monday")) {
    return 1; // Qt::Monday
  }
  if (value == QLatin1String("sunday")) {
    return 7; // Qt::Sunday
  }
  return static_cast<int>(QLocale().firstDayOfWeek());
}

} // namespace

CalendarPreferences::CalendarPreferences(QObject *parent)
    : QObject(parent),
      m_transport(
          std::make_unique<QindaQt::Services::SettingsClient::QtSettingsTransport>(
              QDBusConnection::sessionBus())),
      m_client(
          std::make_unique<QindaQt::Services::SettingsClient::SettingsClient>(
              *m_transport,
              QStringList{QLatin1StringView(keyDefaultView),
                          QLatin1StringView(keyWeekStart),
                          QLatin1StringView(keyDefaultCalendar)})),
      m_defaultView(QLatin1StringView(defaultViewValue)),
      m_weekStart(normalizeWeekStart(QStringLiteral("locale"))),
      m_defaultCalendarId(QLatin1StringView(defaultCalendarIdValue)) {
  connect(m_client.get(),
          &QindaQt::Services::SettingsClient::SettingsClient::snapshotChanged,
          this, &CalendarPreferences::applySnapshot);
  QString error;
  if (!m_client->start(&error)) {
    qInfo().noquote()
        << "QindaQt Calendar settings unavailable, using schema defaults:"
        << error;
  }
}

CalendarPreferences::~CalendarPreferences() = default;

QString CalendarPreferences::defaultView() const { return m_defaultView; }

int CalendarPreferences::weekStart() const { return m_weekStart; }

QString CalendarPreferences::defaultCalendarId() const {
  return m_defaultCalendarId;
}

void CalendarPreferences::applySnapshot() {
  const auto &snapshot = m_client->snapshot();
  if (!snapshot.has_value()) {
    return;
  }
  const QVariantMap &values = snapshot->values;
  const QString nextView =
      normalizeView(values.value(QLatin1String(keyDefaultView)).toString());
  const int nextWeekStart =
      normalizeWeekStart(values.value(QLatin1String(keyWeekStart)).toString());
  const QString nextDefaultId =
      values.value(QLatin1String(keyDefaultCalendar)).toString();
  const QString resolvedDefaultId =
      nextDefaultId.isEmpty() ? QLatin1StringView(defaultCalendarIdValue)
                              : nextDefaultId;
  if (nextView == m_defaultView && nextWeekStart == m_weekStart &&
      resolvedDefaultId == m_defaultCalendarId) {
    return;
  }
  m_defaultView = nextView;
  m_weekStart = nextWeekStart;
  m_defaultCalendarId = resolvedDefaultId;
  emit preferencesChanged();
}

} // namespace QindaQt::Apps::Calendar
