// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_controls/places_controller.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace QindaQt::Shell::DesktopControls {

// The dock's only reach into the filesystem and the File Manager (ADR-0265).
// Pinned paths are data, not commands: the dock controller never stats,
// lists, or opens anything itself. Production (FileManagerDockPaths) goes
// through the File Manager's public FileBoundary and the launcher's bounded
// process seam; tests inject a recording fake.
//
// AGENT-CONTRACT: GUI-thread only. An implementation never invokes a shell,
// never follows a stored path into a command, and revalidates every target
// at the moment it opens it (a folder must still be a directory, a file a
// readable regular file); a refusal never falls back to another program.
class DockPathPort {
public:
  enum class PathKind {
    Missing,
    Directory,
    File,
  };

  virtual ~DockPathPort() = default;

  // What a dropped local path is right now; anything else reads as Missing.
  [[nodiscard]] virtual PathKind classify(const QString &absolutePath) const = 0;
  [[nodiscard]] virtual FolderOpener::Result openFolder(const QString &absolutePath) = 0;
  // Opens a regular file with the desktop's default application.
  [[nodiscard]] virtual FolderOpener::Result openFile(const QString &absolutePath) = 0;
  // At most `limit` visible children of a pinned folder, directories first,
  // then by name: rows {name, path, isDirectory, iconName, device, inode}
  // with device/inode as decimal strings (JavaScript numbers cannot hold
  // every 64-bit identity). Empty plus `diagnostic` when unreadable.
  [[nodiscard]] virtual QVariantList listFolder(const QString &absolutePath, int limit,
                                                QString *diagnostic) const = 0;
  // Opens one row listFolder returned, fenced by the identity it reported.
  [[nodiscard]] virtual FolderOpener::Result openListedEntry(const QVariantMap &entry) = 0;
  // The home Trash's files folder; opening it shows the Trash.
  [[nodiscard]] virtual QString trashFilesDirectory() const = 0;
  // Starts emptying the home Trash. `finished(ok, diagnostic)` runs once on
  // the GUI thread when the File Manager's mutation settles; false (with
  // `diagnostic`) means nothing started and `finished` never runs.
  [[nodiscard]] virtual bool emptyTrash(std::function<void(bool, const QString &)> finished,
                                        QString *diagnostic) = 0;
};

} // namespace QindaQt::Shell::DesktopControls
