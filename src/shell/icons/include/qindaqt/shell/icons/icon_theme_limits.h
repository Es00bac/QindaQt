// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/qtypes.h>

// AGENT-CONTRACT: Shared bounds for the shell iconography module. Every
// filesystem payload (theme indexes, desktop entries, decoded images) crosses
// these ceilings before use; hostile or oversized input fails closed to
// "unresolved" instead of growing shell memory. See
// docs/wiki/shell/iconography.md.

namespace QindaQt::Shell::Icons
{

// Icon names are freedesktop icon names: a bounded ASCII subset with no path
// semantics. Anything longer or outside the character grammar is refused.
inline constexpr qsizetype kMaxIconNameUtf8Bytes = 128;

// index.theme parsing ceilings. An oversized index contributes no directories;
// a directory list longer than the cap is truncated in declared order. The
// freedesktop hicolor index shipped by current distributions already declares
// 649 directories, so this bound must cover that ordinary interoperability
// baseline while remaining independent of hostile index size.
inline constexpr qint64 kMaxThemeIndexBytes = qint64(256) * 1024;
inline constexpr int kMaxThemeDirectories = 1024;

// SVG source payload ceiling: the image provider bounds QSvgRenderer input to
// this many bytes; larger vectors fail closed to the placeholder. Same
// magnitude as the index ceiling, named separately so every bound names its
// consumer.
inline constexpr qint64 kMaxSvgSourceBytes = kMaxThemeIndexBytes;

// Provider request-id ceiling. A URL id longer than this is refused before
// any parsing and before any cache access, so a hostile id contributes
// nothing — not even a cache key.
inline constexpr qsizetype kMaxRequestIdUtf8Bytes = 1024;

// Theme chain bounds: Inherits recursion is cycle-guarded and depth-capped,
// and the flattened search chain (injected themes plus parents plus hicolor)
// never exceeds this length.
inline constexpr int kMaxThemeInheritDepth = 8;
inline constexpr int kMaxThemeChainLength = 16;

// Parsed-index cache bound. When full, the oldest-inserted entries are
// dropped; lookup results never depend on cache residency.
inline constexpr int kMaxCachedThemeIndexes = 64;

// Requested icon geometry bounds. The 512-pixel logical ceiling mirrors the
// status-notifier renderer contract; scale is bounded so device-pixel sizes
// cannot explode.
inline constexpr int kMaxIconLogicalSize = 512;
inline constexpr int kMinIconLogicalSize = 1;
inline constexpr double kMinIconScale = 1.0;
inline constexpr double kMaxIconScale = 4.0;
inline constexpr int kDefaultIconSize = 32;

// Decoded raster source images larger than this are refused before scaling.
inline constexpr int kMaxSourceImageDimension = 2048;

// Provider LRU image cache bound (entries).
inline constexpr int kMaxCachedIconImages = 64;

// Desktop-entry scanning bounds for the resolver.
inline constexpr qint64 kMaxDesktopEntryBytes = qint64(64) * 1024;
inline constexpr int kMaxDesktopEntries = 1024;
inline constexpr int kMaxDesktopEntryDepth = 4;
inline constexpr qsizetype kMaxDesktopEntryIdUtf8Bytes = 255;

} // namespace QindaQt::Shell::Icons
