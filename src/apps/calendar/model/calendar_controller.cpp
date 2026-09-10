// SPDX-License-Identifier: GPL-3.0-or-later
#include "calendar_controller.h"

#include "calendar_collection.h"
#include "event_store.h"
#include "occurrence_expander.h"
#include "occurrence_list_model.h"
#include "reminder_delivery.h"
#include "reminder_scheduler.h"
#include "settings/calendar_preferences.h"
#include "ics/ics_import_export.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/Duration>
#include <KCalendarCore/Event>
#include <KCalendarCore/Recurrence>

#include <QDateTime>
#include <QLocale>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::Calendar {
namespace {

[[nodiscard]] QString localPathFrom(const QString &urlOrPath) {
  const QUrl url(urlOrPath);
  if (url.isValid() && url.isLocalFile()) {
    return url.toLocalFile();
  }
  return urlOrPath;
}

[[nodiscard]] QString slugifyCalendarName(const QString &displayName) {
  QString slug;
  for (const QChar ch : displayName) {
    if (ch.isLetterOrNumber()) {
      slug.append(ch.toLower());
    } else if (!slug.isEmpty() && !slug.endsWith(u'-')) {
      slug.append(u'-');
    }
  }
  while (slug.endsWith(u'-')) {
    slug.chop(1);
  }
  return slug.left(60);
}

} // namespace

CalendarController::CalendarController(QString dataRoot,
                                       CalendarPreferences *preferences,
                                       OccurrenceListModel *occurrenceModel,
                                       QObject *parent)
    : QObject(parent),
      m_dataRoot(std::move(dataRoot)),
      m_preferences(preferences),
      m_occurrenceModel(occurrenceModel),
      m_collection(std::make_unique<CalendarCollection>(m_dataRoot)),
      m_store(std::make_unique<EventStore>(m_dataRoot)),
      m_scheduler(std::make_unique<ReminderScheduler>(
          [] { return QDateTime::currentDateTime(); },
          std::make_unique<QTimerReminderTimer>())),
      m_delivery(std::make_unique<ReminderDelivery>(
          QDBusConnection::sessionBus())),
      m_currentDate(QDate::currentDate()) {
  const CalendarCollectionResult collectionLoad = m_collection->load();
  if (collectionLoad.error == CalendarCollectionError::Absent) {
    // First run: persist the default "Personal" calendar immediately so the
    // collection file and the .ics files stay in sync from the start.
    const CalendarCollectionResult saved = m_collection->save();
    if (!saved.ok()) {
      m_loadError = saved.diagnostic;
    }
  } else if (!collectionLoad.ok()) {
    m_loadError = collectionLoad.diagnostic;
  }
  const EventStoreLoadResult storeLoad =
      m_store->loadAll(m_collection->calendars());
  if (!storeLoad.ok()) {
    const CalendarLoadFailure &first = storeLoad.failures.constFirst();
    m_loadError = QStringLiteral("%1: %2").arg(first.calendarId,
                                               first.diagnostic);
  }

  applyPreferenceDefaults();
  connect(m_preferences, &CalendarPreferences::preferencesChanged, this,
          [this] { applyPreferenceDefaults(); });

  connect(m_scheduler.get(), &ReminderScheduler::reminderDue, this,
          [this](const QString &eventUid, const QString &summary,
                 const QDateTime &occurrenceStart) {
            handleReminderDue(eventUid, summary, occurrenceStart);
          });
  refreshOccurrences();
}

CalendarController::~CalendarController() = default;

void CalendarController::applyPreferenceDefaults() {
  if (!m_preferences) {
    return;
  }
  const QString preferredView = m_preferences->defaultView();
  if (m_viewMode.isEmpty()) {
    m_viewMode = preferredView;
    emit viewModeChanged();
  }
  const int preferredWeekStart = m_preferences->weekStart();
  if (preferredWeekStart != m_weekStart) {
    m_weekStart = preferredWeekStart;
    emit weekStartChanged();
    refreshOccurrences();
  }
}

QDate CalendarController::currentDate() const { return m_currentDate; }

QString CalendarController::viewMode() const { return m_viewMode; }

QVariantList CalendarController::calendars() const {
  QVariantList result;
  for (const CalendarInfo &info : m_collection->calendars()) {
    result.append(QVariantMap{
        {QStringLiteral("id"), info.id},
        {QStringLiteral("displayName"), info.displayName},
        {QStringLiteral("colorToken"), info.colorToken},
        {QStringLiteral("enabled"), info.enabled},
    });
  }
  return result;
}

QString CalendarController::selectedEventUid() const {
  return m_selectedEventUid;
}

QString CalendarController::defaultCalendarId() const {
  return m_collection->defaultCalendarId();
}

int CalendarController::weekStart() const { return m_weekStart; }

QString CalendarController::reminderBannerText() const {
  return m_reminderBannerText;
}

QString CalendarController::loadError() const { return m_loadError; }

QString CalendarController::periodTitle() const {
  const QLocale locale;
  if (m_viewMode == QLatin1String("day")) {
    return locale.toString(m_currentDate, QLocale::LongFormat);
  }
  if (m_viewMode == QLatin1String("week")) {
    const QDate weekStartDate = m_currentDate.addDays(
        -(m_currentDate.dayOfWeek() - m_weekStart + 7) % 7);
    return QStringLiteral("%1 – %2")
        .arg(locale.toString(weekStartDate, QLocale::ShortFormat),
             locale.toString(weekStartDate.addDays(6), QLocale::ShortFormat));
  }
  return QStringLiteral("%1 %2")
      .arg(locale.monthName(m_currentDate.month(), QLocale::LongFormat),
           QString::number(m_currentDate.year()));
}

bool CalendarController::setViewMode(const QString &viewMode) {
  if (viewMode != QLatin1String("month") && viewMode != QLatin1String("week") &&
      viewMode != QLatin1String("day")) {
    emit operationFailed(QStringLiteral("unknown view mode %1").arg(viewMode));
    return false;
  }
  if (viewMode == m_viewMode) {
    return true;
  }
  m_viewMode = viewMode;
  emit viewModeChanged();
  refreshOccurrences();
  return true;
}

void CalendarController::goToday() {
  if (m_currentDate == QDate::currentDate()) {
    return;
  }
  m_currentDate = QDate::currentDate();
  emit currentDateChanged();
  refreshOccurrences();
}

void CalendarController::goToDate(const QString &isoDate) {
  const QDate date = QDate::fromString(isoDate, Qt::ISODate);
  if (!date.isValid() || date == m_currentDate) {
    return;
  }
  m_currentDate = date;
  emit currentDateChanged();
  refreshOccurrences();
}

void CalendarController::dismissReminderBanner() {
  if (m_reminderBannerText.isEmpty()) {
    return;
  }
  m_reminderBannerText.clear();
  emit reminderBannerTextChanged();
}

void CalendarController::previousPeriod() {
  if (m_viewMode == QLatin1String("week")) {
    m_currentDate = m_currentDate.addDays(-7);
  } else if (m_viewMode == QLatin1String("day")) {
    m_currentDate = m_currentDate.addDays(-1);
  } else {
    m_currentDate = m_currentDate.addMonths(-1);
  }
  emit currentDateChanged();
  refreshOccurrences();
}

void CalendarController::nextPeriod() {
  if (m_viewMode == QLatin1String("week")) {
    m_currentDate = m_currentDate.addDays(7);
  } else if (m_viewMode == QLatin1String("day")) {
    m_currentDate = m_currentDate.addDays(1);
  } else {
    m_currentDate = m_currentDate.addMonths(1);
  }
  emit currentDateChanged();
  refreshOccurrences();
}

void CalendarController::selectEvent(const QString &eventUid) {
  if (m_selectedEventUid == eventUid) {
    return;
  }
  m_selectedEventUid = eventUid;
  emit selectedEventUidChanged();
}

bool CalendarController::createEvent(
    const QString &calendarId, const QString &summary, const QString &startIso,
    const QString &endIso, const bool allDay, const QString &location,
    const QString &description, const QString &recurrenceRule,
    const int reminderMinutes) {
  if (summary.trimmed().isEmpty()) {
    emit operationFailed(QStringLiteral("event summary is empty"));
    return false;
  }
  if (!m_store->hasCalendar(calendarId)) {
    emit operationFailed(
        QStringLiteral("unknown calendar %1").arg(calendarId));
    return false;
  }
  const QDateTime start = QDateTime::fromString(startIso, Qt::ISODate);
  const QDateTime end = QDateTime::fromString(endIso, Qt::ISODate);
  if (!start.isValid() || !end.isValid()) {
    emit operationFailed(QStringLiteral("event start or end is invalid"));
    return false;
  }
  if ((!allDay && end <= start) || (allDay && end.date() < start.date())) {
    emit operationFailed(
        QStringLiteral("event end must not precede its start"));
    return false;
  }

  KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
  event->setUid(QUuid::createUuid().toString(QUuid::WithoutBraces) +
                QStringLiteral("@qindaqt.local"));
  event->setSummary(summary.trimmed());
  event->setLocation(location);
  event->setDescription(description);
  event->setDtStart(allDay ? QDateTime(start.date(), QTime(0, 0)) : start);
  event->setDtEnd(allDay ? QDateTime(end.date(), QTime(0, 0)) : end);
  event->setAllDay(allDay);

  if (recurrenceRule == QLatin1String("daily")) {
    event->recurrence()->setDaily(1);
  } else if (recurrenceRule == QLatin1String("weekly")) {
    event->recurrence()->setWeekly(1);
  } else if (recurrenceRule == QLatin1String("monthly")) {
    event->recurrence()->setMonthly(1);
  } else if (recurrenceRule == QLatin1String("yearly")) {
    event->recurrence()->setYearly(1);
  } else if (!recurrenceRule.isEmpty()) {
    emit operationFailed(
        QStringLiteral("unknown recurrence rule %1").arg(recurrenceRule));
    return false;
  }

  if (reminderMinutes >= 0) {
    KCalendarCore::Alarm::Ptr alarm(new KCalendarCore::Alarm(event.data()));
    alarm->setDisplayAlarm(summary.trimmed());
    alarm->setStartOffset(KCalendarCore::Duration(-reminderMinutes * 60));
    alarm->setEnabled(true);
    event->addAlarm(alarm);
  }

  const EventStoreResult stored = m_store->addEvent(calendarId, event);
  if (!stored.ok()) {
    emit operationFailed(stored.diagnostic);
    return false;
  }
  refreshOccurrences();
  return true;
}

QString CalendarController::calendarIdForEvent(const QString &eventUid) const {
  for (const CalendarInfo &info : m_store->calendars()) {
    const auto calendar = m_store->calendar(info.id);
    if (calendar && calendar->event(eventUid)) {
      return info.id;
    }
  }
  return {};
}

bool CalendarController::deleteEvent(const QString &eventUid) {
  const QString calendarId = calendarIdForEvent(eventUid);
  if (calendarId.isEmpty()) {
    emit operationFailed(
        QStringLiteral("unknown event %1").arg(eventUid));
    return false;
  }
  const EventStoreResult removed =
      m_store->removeEvent(calendarId, eventUid);
  if (!removed.ok()) {
    emit operationFailed(removed.diagnostic);
    return false;
  }
  if (m_selectedEventUid == eventUid) {
    m_selectedEventUid.clear();
    emit selectedEventUidChanged();
  }
  refreshOccurrences();
  return true;
}

bool CalendarController::setCalendarEnabled(const QString &calendarId,
                                            const bool enabled) {
  const CalendarCollectionResult result =
      m_collection->setEnabled(calendarId, enabled);
  if (!result.ok()) {
    emit operationFailed(result.diagnostic);
    return false;
  }
  emit calendarsChanged();
  refreshOccurrences();
  return true;
}

QString CalendarController::createCalendar(const QString &displayName) {
  const QString trimmed = displayName.trimmed();
  if (trimmed.isEmpty()) {
    emit operationFailed(QStringLiteral("calendar name is empty"));
    return {};
  }
  QString base = slugifyCalendarName(trimmed);
  if (base.isEmpty()) {
    base = QStringLiteral("calendar");
  }
  QString candidate = base;
  for (int suffix = 2; suffix < 1000; ++suffix) {
    bool taken = false;
    for (const CalendarInfo &existing : m_collection->calendars()) {
      if (existing.id == candidate) {
        taken = true;
        break;
      }
    }
    if (!taken) {
      break;
    }
    candidate = QStringLiteral("%1-%2").arg(base).arg(suffix);
  }

  const CalendarInfo info{.id = candidate,
                          .displayName = trimmed,
                          .colorToken = QStringLiteral("accent"),
                          .enabled = true};
  const CalendarCollectionResult added = m_collection->addCalendar(info);
  if (!added.ok()) {
    emit operationFailed(added.diagnostic);
    return {};
  }
  const EventStoreResult registered = m_store->addCalendar(info);
  if (!registered.ok()) {
    emit operationFailed(registered.diagnostic);
    return {};
  }
  emit calendarsChanged();
  refreshOccurrences();
  return candidate;
}

QVariantMap CalendarController::importIcs(const QString &urlOrPath,
                                          const QString &calendarId) {
  const QString targetId =
      calendarId.isEmpty() ? m_collection->defaultCalendarId() : calendarId;
  const IcsImportResult result =
      importFile(localPathFrom(urlOrPath), targetId, *m_store);
  if (!result.ok()) {
    emit operationFailed(result.error);
    return {{QStringLiteral("ok"), false},
            {QStringLiteral("imported"), 0},
            {QStringLiteral("skippedDuplicates"), 0},
            {QStringLiteral("error"), result.error}};
  }
  refreshOccurrences();
  return {{QStringLiteral("ok"), true},
          {QStringLiteral("imported"), result.imported},
          {QStringLiteral("skippedDuplicates"), result.skippedDuplicates},
          {QStringLiteral("error"), QString()}};
}

QVariantMap CalendarController::exportIcs(const QString &urlOrPath,
                                          const QString &calendarId) {
  const QString path = localPathFrom(urlOrPath);
  const IcsExportResult result =
      calendarId.isEmpty() ? exportAll(path, *m_store)
                           : exportCalendar(calendarId, path, *m_store);
  if (!result.ok()) {
    emit operationFailed(result.error);
    return {{QStringLiteral("ok"), false},
            {QStringLiteral("exported"), 0},
            {QStringLiteral("error"), result.error}};
  }
  return {{QStringLiteral("ok"), true},
          {QStringLiteral("exported"), result.exported},
          {QStringLiteral("error"), QString()}};
}

std::pair<QDateTime, QDateTime> CalendarController::visibleRange() const {
  QDate first;
  QDate last;
  if (m_viewMode == QLatin1String("week")) {
    first = m_currentDate.addDays(
        -(m_currentDate.dayOfWeek() - m_weekStart + 7) % 7);
    last = first.addDays(6);
  } else if (m_viewMode == QLatin1String("day")) {
    first = m_currentDate;
    last = m_currentDate;
  } else {
    first = QDate(m_currentDate.year(), m_currentDate.month(), 1);
    last = first.addMonths(1).addDays(-1);
  }
  return {QDateTime(first, QTime(0, 0)),
          QDateTime(last.addDays(1), QTime(0, 0))};
}

void CalendarController::refreshOccurrences() {
  const auto [rangeStart, rangeEnd] = visibleRange();
  QList<Occurrence> occurrences;
  QList<ReminderRequest> reminders;
  QHash<QString, QString> colorTokens;
  const QDateTime now = QDateTime::currentDateTime();
  const QDateTime reminderStart =
      rangeStart < now ? rangeStart : now;
  const QDateTime reminderEnd =
      rangeEnd > now.addDays(7) ? rangeEnd : now.addDays(7);
  for (const CalendarInfo &info : m_collection->calendars()) {
    colorTokens.insert(info.id, info.colorToken);
    if (!info.enabled) {
      continue;
    }
    const auto calendar = m_store->calendar(info.id);
    if (!calendar) {
      continue;
    }
    occurrences += expandOccurrences(*calendar, info.id, rangeStart, rangeEnd);
    reminders += expandReminders(*calendar, reminderStart, reminderEnd);
  }
  std::sort(occurrences.begin(), occurrences.end(),
            [](const Occurrence &left, const Occurrence &right) {
              if (left.start != right.start) {
                return left.start < right.start;
              }
              return left.eventUid < right.eventUid;
            });
  m_occurrenceModel->resetOccurrences(occurrences, colorTokens);
  m_scheduler->reschedule(reminders);
  emit occurrencesChanged();
}

void CalendarController::handleReminderDue(const QString &eventUid,
                                           const QString &summary,
                                           const QDateTime &occurrenceStart) {
  const QString text =
      QStringLiteral("%1 — %2")
          .arg(summary, occurrenceStart.toString(QStringLiteral("HH:mm")));
  if (m_delivery && m_delivery->deliver(summary, occurrenceStart)) {
    return;
  }
  m_reminderBannerText = text;
  emit reminderBannerTextChanged();
  emit reminderBannerRequested(text);
}

} // namespace QindaQt::Apps::Calendar
