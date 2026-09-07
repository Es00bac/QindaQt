// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>

namespace QindaQt::Apps::FileManager {

struct Bookmark final {
  QString name;
  QString path;

  [[nodiscard]] bool operator==(const Bookmark &) const = default;
};

enum class BookmarksError {
  None,
  Absent,
  InvalidRoot,
  ReadFailed,
  TooLarge,
  Malformed,
  WriteFailed,
};

struct BookmarksLoadResult final {
  QVector<Bookmark> bookmarks;
  BookmarksError error = BookmarksError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == BookmarksError::None; }
};

struct BookmarksWriteResult final {
  BookmarksError error = BookmarksError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == BookmarksError::None; }
};

// Owns only the versioned bookmark inventory. The injected directory is
// selected by composition beneath XDG_STATE_HOME; this class never discovers
// HOME, Settings1, or UI. Every successful write is an atomic same-directory
// replacement containing bookmark names plus absolute paths only. Linux
// openat/O_NOFOLLOW traversal refuses every symlinked directory ancestor, so
// load and store cannot escape the injected root path. Mirrors the Text
// Editor's RestoreStateStore contract (ADR-0065); see ADR-0090.
class BookmarksStore final {
public:
  static constexpr qint64 maximumBytes = 64 * 1024;
  static constexpr int maximumBookmarks = 128;
  static constexpr int maximumPathLength = 4096;
  static constexpr int maximumNameLength = 256;

  explicit BookmarksStore(QString stateDirectory);

  [[nodiscard]] QString filePath() const;
  // Absent is a clean first-run result: ok() is false but bookmarks is empty
  // and diagnostic is empty, so callers show no error on first launch.
  [[nodiscard]] BookmarksLoadResult load() const;
  [[nodiscard]] BookmarksWriteResult store(const QVector<Bookmark> &bookmarks) const;

private:
  [[nodiscard]] static bool validate(const QVector<Bookmark> &bookmarks,
                                     QString *diagnostic);

  QString m_stateDirectory;
};

} // namespace QindaQt::Apps::FileManager
