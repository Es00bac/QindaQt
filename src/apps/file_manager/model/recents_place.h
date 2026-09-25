// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "directory_lister.h"

#include <QString>

// ADR-0272: the Recents place, Finder's Recents. It is browsed exactly like
// the Applications place (applications_place.h): a virtual location the
// window's NavigationController lists through a DirectoryLister decorator, so
// sorting, filtering, selection, Quick Look and opening need nothing new.
namespace QindaQt::Apps::FileManager {

namespace RecentsLocation {

// AGENT-CONTRACT: like ApplicationsLocation::location(), the address has no
// '/' (QDir::cleanPath() keeps it verbatim) and no "://" (NetworkLocation
// classifies it as Local). NavigationHistory gives it no parent and a single
// "Recents" breadcrumb; RecentsDirectoryLister serves its listing;
// PlacesController publishes it; Main.qml's "go.recents" navigates to it
// through PlacesController::recentsLocation. Change it here only.
[[nodiscard]] inline QString location()
{
    return QStringLiteral("recents:");
}

[[nodiscard]] inline bool isLocation(const QString &path)
{
    return path == location();
}

} // namespace RecentsLocation

// AGENT-NOTE: the desktop's one recently-used store, the freedesktop
// desktop-bookmark file $XDG_DATA_HOME/recently-used.xbel. GTK's
// GtkRecentManager and KDE's KRecentDocument (KIO) both write it, so it holds
// what Firefox, Dolphin and KDE and GTK applications opened. File Manager only
// reads it and never keeps a second list (ADR-0272).
[[nodiscard]] QString recentlyUsedStorePath();

// Bounds on one read: the store is written by other programs, so its size and
// the number of rows (each costs one stat on the GUI thread) are capped.
inline constexpr qint64 maximumRecentStoreBytes = 16 * 1024 * 1024;
inline constexpr qsizetype maximumRecentEntries = 300;

// Reads the store at `storePath` into a listing of the local files and
// folders it names that still exist, the most recently used first, at most
// `limit` of them (`truncated` is then set). A bookmark's time is the latest of
// its added, modified and visited stamps, as KRecentDocument reads it; a path
// named twice keeps its latest time. Anything that is not a local absolute
// file: URI is skipped. A missing store is an empty place, not an error; an
// oversized, unreadable or malformed one is a typed ListingError with a
// diagnostic, never a partial list. Bounded synchronous local I/O, any thread.
[[nodiscard]] ListingResult readRecentFiles(const QString &storePath,
                                            qsizetype limit = maximumRecentEntries);

// AGENT-CONTRACT: wraps the window's real lister. A request for
// RecentsLocation::location() is answered by readRecentFiles(storePath);
// every other path goes to `inner` unchanged. Row paths are the files' own
// absolute paths, so the window's launcher, previews and mutations treat a
// recent file exactly as they do in its folder.
class RecentsDirectoryLister final : public DirectoryLister {
public:
  RecentsDirectoryLister(DirectoryListerPtr inner, QString storePath);

  [[nodiscard]] ListingResult list(const QString &absolutePath) const override;

private:
  DirectoryListerPtr m_inner;
  QString m_storePath;
};

} // namespace QindaQt::Apps::FileManager
