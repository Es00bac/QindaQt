// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "calendar_types.h"

#include <QDate>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace QindaQt::Apps::Calendar {

class CalendarCollection;
class CalendarPreferences;
class EventStore;
class OccurrenceListModel;
class ReminderDelivery;
class ReminderScheduler;

// QML-facing bridge for the Calendar app. Owns the collection, the event
// store, the reminder scheduler, and reminder delivery; drives the injected
// OccurrenceListModel. All domain logic lives here or in the model/ layer —
// QML stays presentation-only (module-boundaries rule).
//
// Every mutation path persists through the store/collection (which write
// atomically themselves), then re-expands occurrences and reschedules
// reminders, so QML never triggers persistence or expansion directly.
class CalendarController final : public QObject {
  Q_OBJECT

  Q_PROPERTY(QDate currentDate READ currentDate NOTIFY currentDateChanged)
  Q_PROPERTY(QString viewMode READ viewMode NOTIFY viewModeChanged)
  Q_PROPERTY(QVariantList calendars READ calendars NOTIFY calendarsChanged)
  Q_PROPERTY(QString selectedEventUid READ selectedEventUid WRITE selectEvent
                 NOTIFY selectedEventUidChanged)
  Q_PROPERTY(QString defaultCalendarId READ defaultCalendarId NOTIFY
                 calendarsChanged)
  Q_PROPERTY(int weekStart READ weekStart NOTIFY weekStartChanged)
  Q_PROPERTY(QString reminderBannerText READ reminderBannerText NOTIFY
                 reminderBannerTextChanged)
  Q_PROPERTY(QString periodTitle READ periodTitle NOTIFY currentDateChanged)
  Q_PROPERTY(QString loadError READ loadError CONSTANT)
  // Details of the selected event as a plain map (empty when nothing is
  // selected); re-evaluated whenever the selection or the store changes.
  Q_PROPERTY(QVariantMap selectedEvent READ selectedEvent NOTIFY
                 selectedEventChanged)

public:
  // preferences and occurrenceModel are non-owning and must outlive the
  // controller (composition root stack order guarantees this).
  CalendarController(QString dataRoot, CalendarPreferences *preferences,
                     OccurrenceListModel *occurrenceModel,
                     QObject *parent = nullptr);
  ~CalendarController() override;

  [[nodiscard]] QDate currentDate() const;
  [[nodiscard]] QString viewMode() const;
  [[nodiscard]] QVariantList calendars() const;
  [[nodiscard]] QString selectedEventUid() const;
  [[nodiscard]] QString defaultCalendarId() const;
  [[nodiscard]] int weekStart() const;
  [[nodiscard]] QString reminderBannerText() const;
  [[nodiscard]] QString periodTitle() const;
  [[nodiscard]] QString loadError() const;
  [[nodiscard]] QVariantMap selectedEvent() const;

  [[nodiscard]] Q_INVOKABLE bool setViewMode(const QString &viewMode);
  Q_INVOKABLE void goToday();
  Q_INVOKABLE void goToDate(const QString &isoDate);
  Q_INVOKABLE void previousPeriod();
  Q_INVOKABLE void nextPeriod();
  Q_INVOKABLE void selectEvent(const QString &eventUid);
  Q_INVOKABLE void dismissReminderBanner();

  // recurrenceRule: "" | "daily" | "weekly" | "monthly" | "yearly".
  // reminderMinutes: -1 = no reminder, otherwise minutes before start.
  // startIso/endIso are Qt.ISODate text in local time; for all-day events the
  // end date is inclusive (KCalendarCore convention).
  [[nodiscard]] Q_INVOKABLE bool
  createEvent(const QString &calendarId, const QString &summary,
              const QString &startIso, const QString &endIso, bool allDay,
              const QString &location, const QString &description,
              const QString &recurrenceRule, int reminderMinutes);
  // In-place edit of the event with the given uid: the uid stays stable, the
  // RFC 5545 revision/lastModified are bumped, and recurrence and reminders
  // are replaced wholesale with the given values (recurrenceRule "" clears
  // recurrence, reminderMinutes -1 removes all alarms). This is NOT
  // delete+recreate, so reminder scheduling and occurrence expansion keep
  // their identity invariants.
  [[nodiscard]] Q_INVOKABLE bool
  updateEvent(const QString &eventUid, const QString &summary,
              const QString &startIso, const QString &endIso, bool allDay,
              const QString &location, const QString &description,
              const QString &recurrenceRule, int reminderMinutes);
  [[nodiscard]] Q_INVOKABLE bool deleteEvent(const QString &eventUid);
  [[nodiscard]] Q_INVOKABLE bool setCalendarEnabled(const QString &calendarId,
                                                    bool enabled);
  // Returns the new calendar id, or an empty string on failure.
  [[nodiscard]] Q_INVOKABLE QString createCalendar(const QString &displayName);

  [[nodiscard]] Q_INVOKABLE QVariantMap importIcs(const QString &urlOrPath,
                                                  const QString &calendarId);
  // An empty calendarId exports every calendar into one file.
  [[nodiscard]] Q_INVOKABLE QVariantMap exportIcs(const QString &urlOrPath,
                                                  const QString &calendarId);

signals:
  void currentDateChanged();
  void viewModeChanged();
  void calendarsChanged();
  void selectedEventUidChanged();
  void selectedEventChanged();
  void weekStartChanged();
  void occurrencesChanged();
  void reminderBannerTextChanged();
  void reminderBannerRequested(const QString &text);
  void operationFailed(const QString &message);

private:
  void refreshOccurrences();
  void applyPreferenceDefaults();
  void handleReminderDue(const QString &eventUid, const QString &summary,
                         const QDateTime &occurrenceStart);
  [[nodiscard]] std::pair<QDateTime, QDateTime> visibleRange() const;
  [[nodiscard]] QString calendarIdForEvent(const QString &eventUid) const;

  QString m_dataRoot;
  CalendarPreferences *m_preferences;
  OccurrenceListModel *m_occurrenceModel;
  std::unique_ptr<CalendarCollection> m_collection;
  std::unique_ptr<EventStore> m_store;
  std::unique_ptr<ReminderScheduler> m_scheduler;
  std::unique_ptr<ReminderDelivery> m_delivery;
  QDate m_currentDate;
  QString m_viewMode;
  QString m_selectedEventUid;
  int m_weekStart = 1; // Qt::Monday
  QString m_reminderBannerText;
  QString m_loadError;
};

} // namespace QindaQt::Apps::Calendar
