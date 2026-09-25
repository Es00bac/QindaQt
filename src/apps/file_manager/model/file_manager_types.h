// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QMetaType>
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
  quint64 device = 0;
  quint64 inode = 0;
  qint64 identitySize = 0;
  qint64 modifiedNanoseconds = 0;
  quint32 mode = 0;
  // ADR-0270: the Details view's extra columns. They come from the stat the
  // lister already made, so they cost no extra I/O; a lister that cannot
  // know one leaves it invalid or -1, and the view shows a dash.
  QDateTime created;
  QDateTime accessed;
  qint64 ownerId = -1;
  qint64 groupId = -1;
  // ADR-0262: a row of the Applications place names an installed application
  // rather than a file. Every filesystem entry leaves these four empty.
  QString applicationId; // desktop-entry id; non-empty only for application rows
  QString iconName;      // theme icon; empty derives one from the file name
  QString kindText;      // Kind column text, e.g. the application's category
  QString note;          // why an application row cannot start from here

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

// DirectoryEntry vectors cross the SearchController worker boundary as queued
// signal payloads.
Q_DECLARE_METATYPE(QindaQt::Apps::FileManager::DirectoryEntry)
Q_DECLARE_METATYPE(QVector<QindaQt::Apps::FileManager::DirectoryEntry>)
