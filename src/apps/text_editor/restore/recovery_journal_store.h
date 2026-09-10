// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace QindaQt::Apps::TextEditor {

enum class RecoveryJournalError {
  None,
  Absent,
  InvalidRoot,
  ReadFailed,
  TooLarge,
  Malformed,
  WriteFailed,
};

struct RecoveryJournalEntry final {
  QString key;
  // nullopt path marks an untitled buffer's journal.
  std::optional<QString> path;
  QString text;
};

struct RecoveryJournalLoadResult final {
  std::optional<RecoveryJournalEntry> entry;
  RecoveryJournalError error = RecoveryJournalError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return entry.has_value(); }
};

struct RecoveryJournalWriteResult final {
  RecoveryJournalError error = RecoveryJournalError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == RecoveryJournalError::None; }
};

// AGENT-CONTRACT: The crash-recovery journal is app-local recovery state,
// sibling to the ADR-0065 paths-only inventory and never merged with it: the
// inventory keeps naming documents, journals carry unsaved content. One
// journal file per document key; writes are atomic QSaveFile replacements
// beneath the injected root reached through StateDirectory's
// openat/O_NOFOLLOW walk. Single-writer is process-local: canonical-path
// window uniqueness plus pid-scoped untitled keys keep one writer per key in
// a process; across concurrent processes the last writer wins and no locking
// is implied. Journals for oversized documents are skipped (never truncated);
// a stale earlier journal is kept so recovery still offers the last bounded
// state. Documents the user discarded are deleted and never re-journaled by
// the same window.
class RecoveryJournalStore final {
public:
  static constexpr qint64 maximumJournalBytes = 4LL * 1024LL * 1024LL;
  static constexpr int maximumJournals = 32;

  explicit RecoveryJournalStore(QString rootDirectory);

  [[nodiscard]] QString rootDirectory() const { return m_rootDirectory; }
  // Keys are the only caller-visible identity. Path keys hash the canonical
  // document path; untitled keys embed the process id and a window sequence.
  [[nodiscard]] static QString keyForPath(const QString &canonicalPath);
  [[nodiscard]] static QString keyForUntitled(int sequence);

  [[nodiscard]] bool contains(const QString &key) const;
  [[nodiscard]] RecoveryJournalLoadResult load(const QString &key) const;
  // path empty means untitled. An oversized payload fails with TooLarge and
  // leaves any earlier journal for the key untouched.
  [[nodiscard]] RecoveryJournalWriteResult store(const QString &key,
                                                 const QString &path,
                                                 const QString &text) const;
  [[nodiscard]] RecoveryJournalWriteResult clear(const QString &key) const;
  // Bounded enumeration of valid journals, name-ordered for determinism;
  // malformed entries are skipped (left in place, inert, bounded).
  [[nodiscard]] QList<RecoveryJournalEntry> entries() const;

private:
  [[nodiscard]] static bool isValidKey(const QString &key);
  [[nodiscard]] RecoveryJournalLoadResult
  loadAt(int directoryDescriptor, const QString &key) const;

  QString m_rootDirectory;
};

} // namespace QindaQt::Apps::TextEditor
