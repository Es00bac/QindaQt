// SPDX-License-Identifier: GPL-3.0-or-later
#include "ics_import_export.h"

#include "model/event_store.h"

#include <KCalendarCore/ICalFormat>

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTimeZone>

namespace QindaQt::Apps::Calendar {
namespace {

// Bounds memory use on hostile or accidental input; legitimate exports are
// far below this.
constexpr qint64 maximumIcsBytes = 32 * 1024 * 1024;

[[nodiscard]] bool readIcsFile(const QString &path, QByteArray *payload,
                               QString *error) {
  const QFileInfo info(path);
  if (!info.isFile()) {
    *error = QStringLiteral("%1 is not a file").arg(path);
    return false;
  }
  if (info.size() > maximumIcsBytes) {
    *error = QStringLiteral("%1 exceeds the %2 MiB import limit")
                 .arg(path)
                 .arg(maximumIcsBytes / (1024 * 1024));
    return false;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    *error = file.errorString();
    return false;
  }
  *payload = file.readAll();
  return true;
}

[[nodiscard]] bool writeIcsFile(const QString &path, const QByteArray &payload,
                                QString *error) {
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    *error = file.errorString();
    return false;
  }
  if (file.write(payload) != payload.size() || !file.commit()) {
    *error = file.errorString();
    return false;
  }
  return true;
}

[[nodiscard]] KCalendarCore::MemoryCalendar::Ptr makeScratchCalendar() {
  return KCalendarCore::MemoryCalendar::Ptr(
      new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
}

} // namespace

IcsImportResult importFile(const QString &path, const QString &calendarId,
                           EventStore &store) {
  IcsImportResult result;
  const KCalendarCore::MemoryCalendar::Ptr target = store.calendar(calendarId);
  if (!target) {
    result.error = QStringLiteral("unknown calendar %1").arg(calendarId);
    return result;
  }

  QByteArray payload;
  if (!readIcsFile(path, &payload, &result.error)) {
    return result;
  }
  const KCalendarCore::MemoryCalendar::Ptr parsed = makeScratchCalendar();
  KCalendarCore::ICalFormat format;
  if (!format.fromRawString(parsed, payload)) {
    result.error = QStringLiteral("%1 is not valid iCalendar").arg(path);
    return result;
  }

  KCalendarCore::Event::List fresh;
  for (const KCalendarCore::Event::Ptr &event : parsed->rawEvents()) {
    if (!event || event->uid().isEmpty()) {
      continue;
    }
    if (target->event(event->uid())) {
      ++result.skippedDuplicates;
      continue;
    }
    // Clone: an Incidence belongs to exactly one calendar, so the parsed
    // instance cannot be shared into the target.
    fresh.append(KCalendarCore::Event::Ptr(event->clone()));
  }

  const EventStoreResult stored = store.addEvents(calendarId, fresh);
  if (!stored.ok()) {
    result.error = stored.diagnostic;
    return result;
  }
  result.imported = static_cast<int>(fresh.size());
  return result;
}

IcsExportResult exportCalendar(const QString &calendarId, const QString &path,
                               const EventStore &store) {
  IcsExportResult result;
  const KCalendarCore::MemoryCalendar::Ptr source = store.calendar(calendarId);
  if (!source) {
    result.error = QStringLiteral("unknown calendar %1").arg(calendarId);
    return result;
  }

  const KCalendarCore::MemoryCalendar::Ptr scratch = makeScratchCalendar();
  for (const KCalendarCore::Event::Ptr &event : source->rawEvents()) {
    if (event) {
      scratch->addEvent(KCalendarCore::Event::Ptr(event->clone()));
    }
  }
  KCalendarCore::ICalFormat format;
  if (!writeIcsFile(path, format.toString(scratch).toUtf8(), &result.error)) {
    return result;
  }
  result.exported = static_cast<int>(scratch->rawEvents().size());
  return result;
}

IcsExportResult exportAll(const QString &path, const EventStore &store) {
  IcsExportResult result;
  const KCalendarCore::MemoryCalendar::Ptr scratch = makeScratchCalendar();
  for (const CalendarInfo &info : store.calendars()) {
    const KCalendarCore::MemoryCalendar::Ptr source = store.calendar(info.id);
    if (!source) {
      continue;
    }
    for (const KCalendarCore::Event::Ptr &event : source->rawEvents()) {
      // UIDs are globally unique across the store's calendars in practice;
      // a collision on export-all would corrupt identity, so skip it.
      if (event && !scratch->event(event->uid())) {
        scratch->addEvent(KCalendarCore::Event::Ptr(event->clone()));
      }
    }
  }
  KCalendarCore::ICalFormat format;
  if (!writeIcsFile(path, format.toString(scratch).toUtf8(), &result.error)) {
    return result;
  }
  result.exported = static_cast<int>(scratch->rawEvents().size());
  return result;
}

} // namespace QindaQt::Apps::Calendar
