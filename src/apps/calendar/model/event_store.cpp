// SPDX-License-Identifier: GPL-3.0-or-later
#include "event_store.h"

#include <KCalendarCore/ICalFormat>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTimeZone>

namespace QindaQt::Apps::Calendar {
namespace {

constexpr qsizetype maximumCalendarIdLength = 100;

EventStoreResult storeFailure(const EventStoreError error,
                              const QString &diagnostic) {
  return {.error = error, .diagnostic = diagnostic.left(256)};
}

} // namespace

EventStore::EventStore(QString rootDirectory)
    : m_rootDirectory(std::move(rootDirectory)) {}

QString EventStore::rootDirectory() const { return m_rootDirectory; }

QString EventStore::calendarFilePath(const QString &calendarId) const {
  return m_rootDirectory + u'/' + calendarId + QStringLiteral(".ics");
}

// AGENT-GUARD: Calendar ids become file names beneath the injected root.
// Restricting them to [A-Za-z0-9_-] keeps the store inside its root; any
// broader alphabet must re-audit for path traversal.
bool EventStore::isValidCalendarId(const QString &calendarId) {
  if (calendarId.isEmpty() || calendarId.size() > maximumCalendarIdLength) {
    return false;
  }
  for (const QChar ch : calendarId) {
    const ushort u = ch.unicode();
    const bool ok = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') ||
                    (u >= '0' && u <= '9') || ch == u'-' || ch == u'_';
    if (!ok) {
      return false;
    }
  }
  return true;
}

EventStoreLoadResult EventStore::loadAll(const QList<CalendarInfo> &calendars) {
  EventStoreLoadResult result;
  m_calendars.clear();
  for (const CalendarInfo &info : calendars) {
    if (!isValidCalendarId(info.id)) {
      result.failures.append(
          {info.id, QStringLiteral("calendar id is not a safe identifier")});
      continue;
    }

    auto memoryCalendar = KCalendarCore::MemoryCalendar::Ptr(
        new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    const QString path = calendarFilePath(info.id);
    if (QFileInfo::exists(path)) {
      QFile file(path);
      if (!file.open(QIODevice::ReadOnly)) {
        result.failures.append({info.id, file.errorString()});
      } else {
        // Parse into a scratch calendar and swap it in only on success, so a
        // malformed file can never leave a half-loaded calendar behind.
        auto parsed = KCalendarCore::MemoryCalendar::Ptr(
            new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
        KCalendarCore::ICalFormat format;
        if (format.fromRawString(parsed, file.readAll())) {
          memoryCalendar = parsed;
        } else {
          result.failures.append(
              {info.id, QStringLiteral("calendar file is not valid iCalendar")});
        }
      }
    }
    m_calendars.insert(info.id, CalendarData{info, memoryCalendar});
  }
  return result;
}

EventStoreResult EventStore::addCalendar(const CalendarInfo &info) {
  if (!isValidCalendarId(info.id)) {
    return storeFailure(EventStoreError::InvalidCalendarId,
                        QStringLiteral("calendar id is not a safe identifier"));
  }
  if (m_calendars.contains(info.id)) {
    return storeFailure(EventStoreError::DuplicateCalendar, info.id);
  }
  m_calendars.insert(
      info.id,
      CalendarData{info,
                   KCalendarCore::MemoryCalendar::Ptr(
                       new KCalendarCore::MemoryCalendar(
                           QTimeZone::systemTimeZone()))});
  return {};
}

EventStoreResult EventStore::removeCalendar(const QString &calendarId) {
  const auto it = m_calendars.constFind(calendarId);
  if (it == m_calendars.constEnd()) {
    return storeFailure(EventStoreError::UnknownCalendar, calendarId);
  }
  const QString path = calendarFilePath(calendarId);
  if (QFileInfo::exists(path) && !QFile::remove(path)) {
    return storeFailure(EventStoreError::WriteFailed,
                        QStringLiteral("could not delete %1").arg(path));
  }
  m_calendars.erase(it);
  return {};
}

QList<CalendarInfo> EventStore::calendars() const {
  QList<CalendarInfo> infos;
  infos.reserve(m_calendars.size());
  for (const CalendarData &data : m_calendars) {
    infos.append(data.info);
  }
  return infos;
}

bool EventStore::hasCalendar(const QString &calendarId) const {
  return m_calendars.contains(calendarId);
}

KCalendarCore::MemoryCalendar::Ptr EventStore::calendar(
    const QString &calendarId) const {
  const auto it = m_calendars.constFind(calendarId);
  return it == m_calendars.constEnd() ? KCalendarCore::MemoryCalendar::Ptr()
                                      : it->memoryCalendar;
}

EventStoreResult EventStore::addEvent(
    const QString &calendarId, const KCalendarCore::Event::Ptr &event) {
  return addEvents(calendarId, {event});
}

EventStoreResult EventStore::addEvents(
    const QString &calendarId, const KCalendarCore::Event::List &events) {
  const auto it = m_calendars.find(calendarId);
  if (it == m_calendars.end()) {
    return storeFailure(EventStoreError::UnknownCalendar, calendarId);
  }
  const KCalendarCore::MemoryCalendar::Ptr memoryCalendar = it->memoryCalendar;
  QHash<QString, int> batchUids;
  for (const KCalendarCore::Event::Ptr &event : events) {
    if (!event || event->uid().isEmpty()) {
      return storeFailure(EventStoreError::Malformed,
                          QStringLiteral("event uid is empty"));
    }
    if (memoryCalendar->event(event->uid())) {
      return storeFailure(
          EventStoreError::DuplicateEvent,
          QStringLiteral("uid %1 already exists in %2")
              .arg(event->uid(), calendarId));
    }
    if (++batchUids[event->uid()] > 1) {
      return storeFailure(EventStoreError::DuplicateEvent,
                          QStringLiteral("uid %1 is repeated in the batch")
                              .arg(event->uid()));
    }
  }

  for (const KCalendarCore::Event::Ptr &event : events) {
    if (!memoryCalendar->addEvent(event)) {
      return storeFailure(EventStoreError::Malformed,
                          QStringLiteral("calendar rejected uid %1")
                              .arg(event->uid()));
    }
  }
  const EventStoreResult persisted = persistCalendar(calendarId);
  if (!persisted.ok()) {
    for (const KCalendarCore::Event::Ptr &event : events) {
      memoryCalendar->deleteEvent(event);
    }
    return persisted;
  }
  return {};
}

EventStoreResult EventStore::updateEvent(
    const QString &calendarId, const KCalendarCore::Event::Ptr &event) {
  const auto it = m_calendars.find(calendarId);
  if (it == m_calendars.end()) {
    return storeFailure(EventStoreError::UnknownCalendar, calendarId);
  }
  if (!event || event->uid().isEmpty()) {
    return storeFailure(EventStoreError::Malformed,
                        QStringLiteral("event uid is empty"));
  }
  const KCalendarCore::MemoryCalendar::Ptr memoryCalendar = it->memoryCalendar;
  const KCalendarCore::Event::Ptr previous =
      memoryCalendar->event(event->uid());
  if (!previous) {
    return storeFailure(EventStoreError::UnknownEvent, event->uid());
  }

  memoryCalendar->deleteEvent(previous);
  if (!memoryCalendar->addEvent(event)) {
    memoryCalendar->addEvent(previous);
    return storeFailure(EventStoreError::Malformed,
                        QStringLiteral("calendar rejected uid %1")
                            .arg(event->uid()));
  }
  const EventStoreResult persisted = persistCalendar(calendarId);
  if (!persisted.ok()) {
    memoryCalendar->deleteEvent(event);
    memoryCalendar->addEvent(previous);
    return persisted;
  }
  return {};
}

EventStoreResult EventStore::removeEvent(const QString &calendarId,
                                         const QString &eventUid) {
  const auto it = m_calendars.find(calendarId);
  if (it == m_calendars.end()) {
    return storeFailure(EventStoreError::UnknownCalendar, calendarId);
  }
  const KCalendarCore::MemoryCalendar::Ptr memoryCalendar = it->memoryCalendar;
  const KCalendarCore::Event::Ptr previous = memoryCalendar->event(eventUid);
  if (!previous) {
    return storeFailure(EventStoreError::UnknownEvent, eventUid);
  }

  memoryCalendar->deleteEvent(previous);
  const EventStoreResult persisted = persistCalendar(calendarId);
  if (!persisted.ok()) {
    memoryCalendar->addEvent(previous);
    return persisted;
  }
  return {};
}

KCalendarCore::Event::List EventStore::events(
    const QString &calendarId) const {
  const KCalendarCore::MemoryCalendar::Ptr memoryCalendar =
      calendar(calendarId);
  return memoryCalendar ? memoryCalendar->rawEvents()
                        : KCalendarCore::Event::List();
}

// AGENT-NOTE: KCalendarCore::FileStorage is deliberately not used here. Its
// save() streams straight to the target file (a crash leaves a truncated
// .ics) and its load() mutates the calendar in place before reporting parse
// failure. QSaveFile gives the atomic same-directory replace, and parsing
// into a scratch calendar keeps a malformed file from touching live state.
EventStoreResult EventStore::persistCalendar(const QString &calendarId) {
  const auto it = m_calendars.constFind(calendarId);
  if (it == m_calendars.constEnd()) {
    return storeFailure(EventStoreError::UnknownCalendar, calendarId);
  }
  if (!QDir().mkpath(m_rootDirectory)) {
    return storeFailure(EventStoreError::WriteFailed,
                        QStringLiteral("could not create %1")
                            .arg(m_rootDirectory));
  }

  KCalendarCore::ICalFormat format;
  const QByteArray payload =
      format.toString(it->memoryCalendar).toUtf8();
  QSaveFile file(calendarFilePath(calendarId));
  if (!file.open(QIODevice::WriteOnly)) {
    return storeFailure(EventStoreError::WriteFailed, file.errorString());
  }
  if (file.write(payload) != payload.size()) {
    return storeFailure(EventStoreError::WriteFailed, file.errorString());
  }
  if (!file.commit()) {
    return storeFailure(EventStoreError::WriteFailed, file.errorString());
  }
  return {};
}

} // namespace QindaQt::Apps::Calendar
