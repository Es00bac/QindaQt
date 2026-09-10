// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

#include <functional>

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT: one restorable terminal window is exactly a profile id plus
// a working directory observed from /proc/<pid>/cwd at teardown time (or the
// launch fallback). Scrollback, command text, argv, titles, and environment
// are NEVER persisted here; a crash-stale file therefore carries no session
// content, only paths and profile references.
struct TerminalRestoreEntry final {
  QString profileId;
  QString workingDirectory;

  [[nodiscard]] bool operator==(const TerminalRestoreEntry &) const = default;
};

struct TerminalRestoreEntriesResult final {
  // ok=false means the document was malformed and nothing is trusted.
  bool ok = false;
  // absent=true means no state file exists (a normal first run), still ok.
  bool absent = false;
  QString diagnostic;
  QList<TerminalRestoreEntry> entries;
};

struct TerminalRestoreWriteResult final {
  bool ok = false;
  QString diagnostic;
};

constexpr int kMaxTerminalRestoreEntries = 16;
constexpr int kMaxTerminalRestorePathLength = 4096;

// Canonical JSON codec for the state file. Encode refuses over-bound lists.
// Decode is fail-closed on a malformed document (ok=false, consumer treats as
// empty) and drops individually hostile entries (unsafe profile id, relative
// or oversized or NUL-bearing path). Unknown JSON fields are ignored.
[[nodiscard]] QString
encodeTerminalRestoreEntries(const QList<TerminalRestoreEntry> &entries,
                             bool *ok);
[[nodiscard]] TerminalRestoreEntriesResult
decodeTerminalRestoreEntries(const QString &json);

// Clean-exit merge: the closed window is appended, an existing record with the
// same profile+directory is removed first (most-recent position wins), and
// the oldest records are evicted beyond the bound.
[[nodiscard]] QList<TerminalRestoreEntry> terminalRestoreWithExitAppended(
    QList<TerminalRestoreEntry> recorded, const TerminalRestoreEntry &closed);

// Launch-time plan over recorded entries: the first admissible entry opens in
// this process, the rest are dispatched as separate Terminal processes.
// Entries whose profile id is unknown or whose directory no longer exists are
// skipped. Both predicates are injected so the policy is testable without a
// live session, Settings1, or filesystem state.
struct TerminalRestorePlan final {
  bool hasPrimary = false;
  TerminalRestoreEntry primary;
  QList<TerminalRestoreEntry> dispatched;
};
[[nodiscard]] TerminalRestorePlan planTerminalRestore(
    const QList<TerminalRestoreEntry> &recorded,
    const std::function<bool(const QString &)> &profileIdKnown,
    const std::function<bool(const QString &)> &directoryExists);

// Bounded on-disk store beneath an injected state directory, holding only
// TerminalRestoreEntry values. Every write is an atomic same-directory
// replacement (QSaveFile); load of an absent file is absent=true, not an
// error; an unreadable, oversized, or malformed file is a typed failure that
// callers must treat as "nothing to restore". The store touches only its own
// fixed file name beneath the injected root.
class TerminalRestoreStore final {
public:
  static constexpr qint64 maximumBytes = 64 * 1024;

  explicit TerminalRestoreStore(QString stateDirectory);

  [[nodiscard]] QString filePath() const;
  [[nodiscard]] TerminalRestoreEntriesResult load() const;
  [[nodiscard]] TerminalRestoreWriteResult
  store(const QList<TerminalRestoreEntry> &entries) const;
  [[nodiscard]] TerminalRestoreWriteResult clear() const;

private:
  QString m_stateDirectory;
};

} // namespace QindaQt::Apps::Terminal
