// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

namespace QindaQt::Apps::FileManager {

// One local directory child. Directory status already follows a symlink
// target (QFileInfo::isDir() does), matching ordinary file-manager navigation
// where a symlink to a folder behaves as a folder.
struct DirectoryEntry final {
  QString name;
  QString absolutePath;
  bool isDirectory = false;
  bool isSymlink = false;
  bool isHidden = false;
  bool isReadable = true;
  qint64 size = 0;
  QDateTime lastModified;

  [[nodiscard]] bool operator==(const DirectoryEntry &) const = default;
};

enum class ListingError {
  None,
  NotFound,
  NotADirectory,
  PermissionDenied,
  Unknown,
};

struct ListingResult final {
  QString path;
  QVector<DirectoryEntry> entries;
  // True when a bounded lister stopped before enumerating every child. See
  // LocalDirectoryLister::maximumEntries.
  bool truncated = false;
  ListingError error = ListingError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == ListingError::None; }
};

} // namespace QindaQt::Apps::FileManager
