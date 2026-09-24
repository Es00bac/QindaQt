// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "directory_lister.h"
#include "launch_intent.h"

#include <functional>

namespace QindaQt::Apps::FileManager {

// ADR-0262: the Applications place reaches the window's NavigationController
// through the two seams that controller already owns -- its DirectoryLister
// and its FileLauncher -- so the place is browsed, sorted, filtered,
// selected, zoomed and activated exactly like a folder, and the controller
// itself learns nothing about applications.

// AGENT-CONTRACT: wraps the window's real lister. A request for
// ApplicationsLocation::location() is answered by `applications` (production:
// ApplicationsController::refresh() then listing(), synchronous and bounded
// by the catalog scan's ceilings, like every DirectoryLister); every other
// path goes to `inner` unchanged. The source must outlive this lister.
class ApplicationsDirectoryLister final : public DirectoryLister {
public:
  using Source = std::function<ListingResult()>;

  ApplicationsDirectoryLister(DirectoryListerPtr inner, Source applications);

  [[nodiscard]] ListingResult list(const QString &absolutePath) const override;

private:
  DirectoryListerPtr m_inner;
  Source m_applications;
};

// AGENT-CONTRACT: wraps the window's real launcher. An application row
// (ApplicationsLocation::entryPath) is handed, by desktop-entry id, to
// `open` (production: ApplicationsController::open, the ADR-0164/0165/0172
// launch authority), which returns its failure text or an empty string; any
// failure becomes a LaunchFailed result for the window's launch banner. Every
// other path goes to `inner` -- ADR-0029's bounded document launch --
// unchanged. This class never reads or executes a desktop entry itself. The
// opener must outlive this launcher.
class ApplicationsFileLauncher final : public FileLauncher {
public:
  using Opener = std::function<QString(const QString &entryId)>;

  ApplicationsFileLauncher(FileLauncherPtr inner, Opener open);

  [[nodiscard]] LaunchResult launch(const QString &absolutePath) const override;

private:
  FileLauncherPtr m_inner;
  Opener m_open;
};

} // namespace QindaQt::Apps::FileManager
