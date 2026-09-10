// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Apps::Calendar {

class EventStore;

struct IcsImportResult final {
  int imported = 0;
  int skippedDuplicates = 0;
  QString error;

  [[nodiscard]] bool ok() const { return error.isEmpty(); }
};

struct IcsExportResult final {
  int exported = 0;
  QString error;

  [[nodiscard]] bool ok() const { return error.isEmpty(); }
};

// Imports an RFC 5545 file into one calendar. The file is parsed into a
// scratch calendar first; only a fully valid parse touches the store, and
// all non-duplicate events are then added and persisted as one batch. An
// event whose UID already exists in the target is skipped and counted in
// skippedDuplicates (import is the merge path — UID conflicts are skipped,
// never overwritten). Non-event components (VTODO, VJOURNAL) are ignored.
[[nodiscard]] IcsImportResult importFile(const QString &path,
                                         const QString &calendarId,
                                         EventStore &store);

// Writes one calendar as .ics. The target file is replaced atomically.
[[nodiscard]] IcsExportResult exportCalendar(const QString &calendarId,
                                             const QString &path,
                                             const EventStore &store);

// Writes every calendar in the store into a single combined .ics file.
[[nodiscard]] IcsExportResult exportAll(const QString &path,
                                        const EventStore &store);

} // namespace QindaQt::Apps::Calendar
