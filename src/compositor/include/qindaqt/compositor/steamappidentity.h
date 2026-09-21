// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::Compositor {

// AGENT-CONTRACT: bounded readers for Steam's ACF/VDF metadata, the Gap-1 half
// of ADR-0230 layered on ADR-0169. A `steam_app_<n>` window class is only a
// numeric key; the human name lives in `steamapps/appmanifest_<n>.acf`, and
// the library roots live in `steamapps/libraryfolders.vdf`. Both files are
// attacker-adjacent input (not authored by us, synced from content servers),
// so every reader here is pure, capped in bytes, nesting, and pair count, and
// REJECTS malformed input instead of guessing. Nothing here touches the
// filesystem; callers feed bytes in.
//
// The grammar accepted is Valve's nested key-quote-value superset:
//   "key" "value"   |   "key" { ...pairs... }
// with backslash escapes inside strings (`\\` and `\"` decode, any other
// escaped character is kept literally). Comments, bare tokens, unterminated
// strings, and trailing garbage are all rejected (fail closed).

// Ceilings shared by both readers. One MiB covers every observed appmanifest
// (tens of KiB) and libraryfolders (a few KiB) with headroom; depth and pair
// caps stop nesting and fan-out bombs from growing compositor memory.
inline constexpr qint64 kMaxVdfBytes = qint64(1024) * 1024;
inline constexpr int kMaxVdfDepth = 8;
inline constexpr int kMaxVdfPairs = 4096;
inline constexpr int kMaxVdfTokenUtf8Bytes = 1024;
inline constexpr int kMaxSteamLibraryRoots = 16;
inline constexpr int kMaxSteamNameCharacters = 256;

// The numeric id behind a `steam_app_<digits>` class, or nullopt for any
// other value (including `steam_app_` with no digits, embedded junk, or an
// overflowing digit run). Case-insensitive, matching isOpaqueLauncherClass.
[[nodiscard]] std::optional<quint64> steamAppIdFromClass(const QString &resourceClass);

// Library root paths declared by a `libraryfolders.vdf` payload: every entry,
// whether the modern nested form (`"0" { "path" "..." }`) or the legacy flat
// form (`"0" "..."`), contributes its path in declared order, capped at
// kMaxSteamLibraryRoots. Returns false on any malformed input, in which case
// `paths` is left empty - a bad file must never yield half a root list.
[[nodiscard]] bool parseSteamLibraryFolders(const QByteArray &vdf,
                                            QStringList *paths);

// The `"name"` an `appmanifest_<id>.acf` declares under its top-level
// `"AppState"` object. Empty when the document is malformed, oversized, has
// no usable name, or the name carries control characters (which could reach
// presentation). Names longer than kMaxSteamNameCharacters are refused, not
// truncated - a hostile file gets nothing.
[[nodiscard]] QString parseSteamAppManifestName(const QByteArray &acf);

} // namespace QindaQt::Compositor
