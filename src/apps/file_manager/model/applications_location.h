// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// The Applications place's address (ADR-0262). Header-only so the navigation
// history, the listing decorator, the action bindings and the applications
// controller all compare against one spelling without a link dependency.
namespace QindaQt::Apps::FileManager::ApplicationsLocation {

// AGENT-CONTRACT: the Applications place is a virtual location the window's
// NavigationController browses exactly like a folder. The address contains no
// '/', so QDir::cleanPath() keeps it verbatim (and folds "applications:/" or
// "applications:///" typed into the location bar onto it), and
// NetworkLocation::classify() treats it as Local because it has no "://".
// NavigationHistory gives it no parent and a single "Applications"
// breadcrumb; ApplicationsDirectoryLister serves its listing; Main.qml reads
// it through ApplicationsController::location. Change it in one place only.
[[nodiscard]] inline QString location()
{
    return QStringLiteral("applications:");
}

[[nodiscard]] inline bool isLocation(const QString &path)
{
    return path == location();
}

// A listed application's row path: the location followed by its
// desktop-entry id. It is never a filesystem path, so DesktopFileLauncher and
// the mutation pipeline refuse it (fail closed) if a row ever reaches them;
// the views route application rows to ApplicationsController instead.
[[nodiscard]] inline QString entryPath(const QString &entryId)
{
    return location() + entryId;
}

} // namespace QindaQt::Apps::FileManager::ApplicationsLocation
