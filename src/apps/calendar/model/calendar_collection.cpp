// SPDX-License-Identifier: GPL-3.0-or-later
#include "calendar_collection.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

namespace QindaQt::Apps::Calendar {
namespace {

constexpr auto collectionFileName = "calendars.json";
constexpr int collectionVersion = 1;
constexpr int maximumCalendars = 64;
constexpr int maximumDisplayNameLength = 256;
constexpr qsizetype maximumCalendarIdLength = 100;

CalendarCollectionResult collectionFailure(
    const CalendarCollectionError error, const QString &diagnostic) {
  return {.error = error, .diagnostic = diagnostic.left(256)};
}

[[nodiscard]] CalendarInfo defaultPersonalCalendar() {
  return {.id = QStringLiteral("personal"),
          .displayName = QStringLiteral("Personal"),
          .colorToken = QStringLiteral("accent"),
          .enabled = true};
}

} // namespace

CalendarCollection::CalendarCollection(QString rootDirectory)
    : m_rootDirectory(std::move(rootDirectory)) {}

QString CalendarCollection::rootDirectory() const { return m_rootDirectory; }

QString CalendarCollection::filePath() const {
  return m_rootDirectory + u'/' + QLatin1String(collectionFileName);
}

bool CalendarCollection::isValidCalendarId(const QString &calendarId) {
  // Must stay identical to EventStore's rule: the id names the .ics file.
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

int CalendarCollection::indexOf(const QString &calendarId) const {
  for (int i = 0; i < m_calendars.size(); ++i) {
    if (m_calendars.at(i).id == calendarId) {
      return i;
    }
  }
  return -1;
}

QList<CalendarInfo> CalendarCollection::calendars() const {
  return m_calendars;
}

QString CalendarCollection::defaultCalendarId() const {
  return m_defaultCalendarId;
}

CalendarCollectionResult CalendarCollection::load() {
  if (!QFileInfo::exists(filePath())) {
    m_calendars = {defaultPersonalCalendar()};
    m_defaultCalendarId = m_calendars.first().id;
    return collectionFailure(CalendarCollectionError::Absent, QString());
  }

  QFile file(filePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return collectionFailure(CalendarCollectionError::ReadFailed,
                             file.errorString());
  }
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll());
  const QJsonObject root = document.object();
  if (!document.isObject() ||
      root.value(QStringLiteral("version")).toInt() != collectionVersion ||
      !root.value(QStringLiteral("calendars")).isArray()) {
    return collectionFailure(
        CalendarCollectionError::Malformed,
        QStringLiteral("calendars.json is not a version 1 collection"));
  }

  QList<CalendarInfo> parsed;
  QSet<QString> seenIds;
  const QJsonArray entries = root.value(QStringLiteral("calendars")).toArray();
  if (entries.isEmpty() || entries.size() > maximumCalendars) {
    return collectionFailure(CalendarCollectionError::Malformed,
                             QStringLiteral("calendar count out of bounds"));
  }
  for (const QJsonValue &entryValue : entries) {
    const QJsonObject entry = entryValue.toObject();
    CalendarInfo info;
    info.id = entry.value(QStringLiteral("id")).toString();
    info.displayName = entry.value(QStringLiteral("displayName")).toString();
    info.colorToken = entry.value(QStringLiteral("colorToken")).toString();
    info.enabled = entry.value(QStringLiteral("enabled")).toBool(true);
    if (!isValidCalendarId(info.id) || info.displayName.isEmpty() ||
        info.displayName.size() > maximumDisplayNameLength || seenIds.contains(info.id)) {
      return collectionFailure(
          CalendarCollectionError::Malformed,
          QStringLiteral("invalid calendar entry %1").arg(info.id));
    }
    seenIds.insert(info.id);
    parsed.append(info);
  }

  const QString parsedDefaultId =
      root.value(QStringLiteral("defaultCalendarId")).toString();
  if (!seenIds.contains(parsedDefaultId)) {
    return collectionFailure(
        CalendarCollectionError::Malformed,
        QStringLiteral("default calendar %1 is not in the collection")
            .arg(parsedDefaultId));
  }

  m_calendars = parsed;
  m_defaultCalendarId = parsedDefaultId;
  return {};
}

CalendarCollectionResult CalendarCollection::save() const { return persist(); }

CalendarCollectionResult CalendarCollection::addCalendar(
    const CalendarInfo &info) {
  if (!isValidCalendarId(info.id)) {
    return collectionFailure(
        CalendarCollectionError::InvalidCalendarId,
        QStringLiteral("calendar id is not a safe identifier"));
  }
  if (info.displayName.isEmpty() ||
      info.displayName.size() > maximumDisplayNameLength) {
    return collectionFailure(CalendarCollectionError::Malformed,
                             QStringLiteral("display name out of bounds"));
  }
  if (indexOf(info.id) >= 0) {
    return collectionFailure(CalendarCollectionError::DuplicateCalendar,
                             info.id);
  }
  m_calendars.append(info);
  const CalendarCollectionResult persisted = persist();
  if (!persisted.ok()) {
    m_calendars.removeLast();
    return persisted;
  }
  return {};
}

CalendarCollectionResult CalendarCollection::removeCalendar(
    const QString &calendarId) {
  const int index = indexOf(calendarId);
  if (index < 0) {
    return collectionFailure(CalendarCollectionError::UnknownCalendar,
                             calendarId);
  }
  if (calendarId == m_defaultCalendarId) {
    return collectionFailure(
        CalendarCollectionError::CannotRemoveDefaultCalendar, calendarId);
  }
  const CalendarInfo removed = m_calendars.at(index);
  m_calendars.removeAt(index);
  const CalendarCollectionResult persisted = persist();
  if (!persisted.ok()) {
    m_calendars.insert(index, removed);
    return persisted;
  }
  return {};
}

CalendarCollectionResult CalendarCollection::renameCalendar(
    const QString &calendarId, const QString &displayName) {
  const int index = indexOf(calendarId);
  if (index < 0) {
    return collectionFailure(CalendarCollectionError::UnknownCalendar,
                             calendarId);
  }
  if (displayName.isEmpty() || displayName.size() > maximumDisplayNameLength) {
    return collectionFailure(CalendarCollectionError::Malformed,
                             QStringLiteral("display name out of bounds"));
  }
  const QString previous = m_calendars[index].displayName;
  m_calendars[index].displayName = displayName;
  const CalendarCollectionResult persisted = persist();
  if (!persisted.ok()) {
    m_calendars[index].displayName = previous;
    return persisted;
  }
  return {};
}

CalendarCollectionResult CalendarCollection::setEnabled(
    const QString &calendarId, const bool enabled) {
  const int index = indexOf(calendarId);
  if (index < 0) {
    return collectionFailure(CalendarCollectionError::UnknownCalendar,
                             calendarId);
  }
  const bool previous = m_calendars[index].enabled;
  m_calendars[index].enabled = enabled;
  const CalendarCollectionResult persisted = persist();
  if (!persisted.ok()) {
    m_calendars[index].enabled = previous;
    return persisted;
  }
  return {};
}

CalendarCollectionResult CalendarCollection::setDefaultCalendarId(
    const QString &calendarId) {
  if (indexOf(calendarId) < 0) {
    return collectionFailure(CalendarCollectionError::UnknownCalendar,
                             calendarId);
  }
  const QString previous = m_defaultCalendarId;
  m_defaultCalendarId = calendarId;
  const CalendarCollectionResult persisted = persist();
  if (!persisted.ok()) {
    m_defaultCalendarId = previous;
    return persisted;
  }
  return {};
}

CalendarCollectionResult CalendarCollection::persist() const {
  QJsonArray entries;
  for (const CalendarInfo &info : m_calendars) {
    entries.append(QJsonObject{{QStringLiteral("id"), info.id},
                               {QStringLiteral("displayName"), info.displayName},
                               {QStringLiteral("colorToken"), info.colorToken},
                               {QStringLiteral("enabled"), info.enabled}});
  }
  const QJsonObject root{
      {QStringLiteral("version"), collectionVersion},
      {QStringLiteral("defaultCalendarId"), m_defaultCalendarId},
      {QStringLiteral("calendars"), entries},
  };

  if (!QDir().mkpath(m_rootDirectory)) {
    return collectionFailure(CalendarCollectionError::WriteFailed,
                             QStringLiteral("could not create %1")
                                 .arg(m_rootDirectory));
  }
  QSaveFile file(filePath());
  if (!file.open(QIODevice::WriteOnly)) {
    return collectionFailure(CalendarCollectionError::WriteFailed,
                             file.errorString());
  }
  const QByteArray payload =
      QJsonDocument(root).toJson(QJsonDocument::Compact);
  if (file.write(payload) != payload.size() || !file.commit()) {
    return collectionFailure(CalendarCollectionError::WriteFailed,
                             file.errorString());
  }
  return {};
}

} // namespace QindaQt::Apps::Calendar
