// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

// ADR-0273: one File Manager window's share of a "show in folder" request --
// the folder it shows and the entries it selects there.
struct RevealRequest final {
  // Canonical absolute directory, readable and enterable when planned.
  QString folder;
  // Plain names of entries of `folder`, in request order and without
  // duplicates. Empty to show the folder with nothing selected.
  QStringList names;
  // Also open the properties dialog for the selected entries.
  bool showProperties = false;

  friend bool operator==(const RevealRequest &, const RevealRequest &) = default;
};

// The three org.freedesktop.FileManager1 methods.
enum class RevealKind { Folders, Items, ItemProperties };

enum class RevealError {
  None,
  // Not a file: URI of a local absolute path (another scheme, a remote host,
  // user information, a port, a query or a fragment).
  NotLocal,
  // The folder or the entry does not exist.
  NotFound,
  // ShowFolders named something that is not a directory.
  NotDirectory,
  // The folder cannot be listed or entered.
  Unreadable,
  // More URIs, or more windows, than one call may ask for.
  TooMany,
};

struct RevealPlan final {
  RevealError error = RevealError::None;
  QString diagnostic;
  QList<RevealRequest> requests;

  [[nodiscard]] bool ok() const { return error == RevealError::None; }
};

// Bounds on one call: a caller on the session bus cannot make File Manager
// stat an unbounded list or open an unbounded number of windows.
inline constexpr qsizetype maximumRevealUris = 256;
inline constexpr qsizetype maximumRevealWindows = 8;

// True for the name of one directory entry: not empty, not "." or "..", and
// without '/' or NUL.
[[nodiscard]] bool isRevealableName(const QString &name);

// Validates every URI of one org.freedesktop.FileManager1 call before anything
// is shown, then groups them into windows: one per distinct folder, in the
// order the folders first appear. A folder must resolve once to a readable,
// enterable directory (FileBoundary::openLocalFolder's rule). An item must
// exist in such a folder; a symbolic link is the entry itself, even when it
// dangles. One bad URI refuses the whole call, so the caller falls back to
// its own behaviour instead of seeing part of its request. Pure policy over
// bounded synchronous stat calls; no listing, no D-Bus, any thread.
[[nodiscard]] RevealPlan planReveal(RevealKind kind, const QStringList &uris);

} // namespace QindaQt::Apps::FileManager
