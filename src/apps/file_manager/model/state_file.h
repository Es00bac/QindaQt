// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: the one symlink-refusing, size-bounded, atomically
// replaced app-local state file primitive shared by every File Manager
// store beneath $XDG_STATE_HOME (ADR-0090). It owns the Linux
// openat/O_NOFOLLOW directory traversal, the regular-file check, the size
// bound, and the same-directory QSaveFile commit; it owns no schema, no
// JSON, and no human-readable message.
//
// AGENT-GUARD: the directory descriptor is reached component by component
// with O_NOFOLLOW and stays open through commit, so no symlinked ancestor
// and no replaced final entry can redirect a store outside its injected
// root. Every caller must keep mapping Error to its own diagnostics rather
// than leaking a raw path into the UI.
//
// Callers own the schema: BookmarksStore (bookmarks-v1.json) and
// NetworkLocationsStore (network-locations-v1.json) both compose one of
// these and translate Error into their own typed error enum, so their
// existing diagnostics and first-run (Absent) behavior are unchanged.
class StateFile final {
public:
  enum class Error {
    None,
    // The state directory or the file itself does not exist yet. A clean
    // first-run result, never an error to show.
    Absent,
    // The state root is missing a component, is not a directory, or a
    // component is a symlink. The store refuses rather than following it.
    InvalidRoot,
    // The final entry exists but is not a regular file (or O_NOFOLLOW
    // refused it), so the store will neither read nor replace it.
    NotRegular,
    ReadFailed,
    TooLarge,
    WriteFailed,
  };

  struct ReadResult final {
    QByteArray bytes;
    Error error = Error::None;
    // Only set for ReadFailed: the QFile error string, for the owner to
    // bound and present. Empty for every other outcome.
    QString systemDiagnostic;

    [[nodiscard]] bool ok() const { return error == Error::None; }
  };

  struct WriteResult final {
    Error error = Error::None;
    // Only set for WriteFailed: the QSaveFile error string.
    QString systemDiagnostic;

    [[nodiscard]] bool ok() const { return error == Error::None; }
  };

  // directory is an absolute path beneath $XDG_STATE_HOME chosen by
  // composition; fileName is one plain file name (no separator);
  // maximumBytes bounds both directions.
  StateFile(QString directory, QByteArray fileName, qint64 maximumBytes);

  [[nodiscard]] QString filePath() const;
  // Never creates the directory: an absent root is Absent, not an error.
  [[nodiscard]] ReadResult read() const;
  // Creates the directory chain (0700) when absent, then commits bytes
  // atomically in the same directory.
  [[nodiscard]] WriteResult write(const QByteArray &bytes) const;

private:
  QString m_directory;
  QByteArray m_fileName;
  qint64 m_maximumBytes;
};

} // namespace QindaQt::Apps::FileManager
