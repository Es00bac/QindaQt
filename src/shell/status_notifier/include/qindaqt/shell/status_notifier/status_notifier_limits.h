// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::StatusNotifier
{

inline constexpr quint32 kSchemaVersion = 1;

// AGENT-CONTRACT: These bounds are the shared ceiling for every future
// StatusNotifier source (the QtDBus adapter) and the shell tray consumer.
// Producers must enforce them before handing values to the registry, and
// consumers must never assume values beyond them. Changing a bound is a
// cross-module contract change, not a local edit.
inline constexpr qsizetype kMaxItems = 64;
// AGENT-GUARD: Only live owners occupy tracking slots. Raising or lowering
// this bound changes a cross-module resource contract; exhaustion fails
// closed (beginOwnerGeneration returns 0) rather than evicting a live owner.
inline constexpr qsizetype kMaxTrackedOwners = 256;
inline constexpr qsizetype kMaxUniqueNameUtf8Bytes = 255;
inline constexpr qsizetype kMaxObjectPathUtf8Bytes = 255;
inline constexpr qsizetype kMaxIdentityUtf8Bytes = 256;
inline constexpr qsizetype kMaxTitleUtf8Bytes = 256;
inline constexpr qsizetype kMaxTextUtf8Bytes = 512;
inline constexpr qsizetype kMaxIconNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxMovieNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxShortcutUtf8Bytes = 64;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 256;
inline constexpr qsizetype kMaxIconPixmaps = 8;
inline constexpr quint32 kMaxIconPixmapDimension = 512;
inline constexpr qsizetype kMaxIconPixmapBytes = qsizetype(1024) * 1024;
inline constexpr qsizetype kMaxToolTipPixmaps = 2;
inline constexpr qsizetype kMaxMenuNodes = 128;
inline constexpr int kMaxMenuDepth = 4;
// AGENT-CONTRACT: Icon-rendering budgets shared by the theme locator and any
// future producer. Theme index files and decoded icon files are hostile input
// too; reads beyond these budgets fail closed instead of exhausting memory.
inline constexpr qsizetype kMaxIconThemeIndexBytes = qsizetype(64) * 1024;
inline constexpr qsizetype kMaxIconThemeDirectories = 64;
inline constexpr qsizetype kMaxIconFileBytes = qsizetype(8) * 1024 * 1024;

// StatusNotifier protocol names are shared by the pure foundation and the
// production watcher/item-client adapters. Defining the constants here does
// not reverse the boundary: this module never opens a connection, owns a
// name, or contacts a service.
inline constexpr char kWatcherServiceName[] = "org.kde.StatusNotifierWatcher";
inline constexpr char kWatcherObjectPath[] = "/StatusNotifierWatcher";
inline constexpr char kWatcherInterfaceName[] = "org.kde.StatusNotifierWatcher";
inline constexpr char kItemInterfaceName[] = "org.kde.StatusNotifierItem";

} // namespace QindaQt::StatusNotifier
