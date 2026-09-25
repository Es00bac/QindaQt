// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace QindaQt::QindaLutris::StoreIo {

// AGENT-NOTE: the file-level half of every QindaLutris app-local document
// (ADR-0231 on ADR-0198, extended by ADR-0275 for titles-v1.json). It was
// private to library_store.cpp until the title store needed the same rules;
// it was extracted rather than copied so the two stores cannot drift apart
// on symlink, size or atomicity handling. Internal to the model library:
// schema knowledge stays in each store, only byte and string discipline
// lives here.

enum class ReadStatus {
  Ok,
  Absent,  // no file (or a zero-byte file): first run, not a failure
  Refused, // not a regular file, a symlink, oversized, or unreadable
};

// AGENT-GUARD: refuses symlinks (opened O_NOFOLLOW, so there is no
// check-then-open race), non-regular files and documents over maxBytes
// before a byte is parsed. A zero-byte file reports Absent, which is what
// the original LibraryStore reader did; keep that so both stores agree.
// POSIX only, like the rest of QindaQt.
[[nodiscard]] QByteArray readBoundedFile(const QString &path, qint64 maxBytes,
                                         ReadStatus *status);

// AGENT-GUARD: commits through QSaveFile in the destination directory, so a
// crash leaves either the old document or the new one, never a torn one.
// Refuses (false) when the destination exists and is not a regular file --
// in particular a symlink, which QSaveFile would otherwise write through.
[[nodiscard]] bool writeAtomicJson(const QString &path,
                                   const QJsonDocument &document);

// A JSON string no longer than maxChars with no control characters except
// '\n'. *ok is false on any other shape; the caller refuses the document.
[[nodiscard]] QString boundedString(const QJsonValue &value, int maxChars,
                                    bool *ok);

// As boundedString, but refuses '\n' as well: for single-line values such as
// paths, ids and argv entries.
[[nodiscard]] QString boundedLine(const QJsonValue &value, int maxChars,
                                  bool *ok);

// A JSON array of at most maxEntries boundedLine strings.
[[nodiscard]] QStringList boundedLineList(const QJsonValue &value,
                                          int maxEntries, int maxChars,
                                          bool *ok);

// True when the text is short enough and free of control characters.
[[nodiscard]] bool isCleanLine(const QString &text, int maxChars);

} // namespace QindaQt::QindaLutris::StoreIo
