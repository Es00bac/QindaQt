// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QtGlobal>

namespace QindaQt::ShellLauncher::Bounds {

// AGENT-CONTRACT: These limits are the launcher's hostile-input boundary.
// Producers (a later installed-application adapter) and consumers (shell QML)
// must both rely on them; duplicating any limit outside this header creates a
// second authority that can drift. Mirrors the bounded-value style of the
// display and audio protocol modules.

// Desktop-entry documents come from deterministic callers in L0 and from a
// scanned provider later; both face the same per-document ceiling.
// The public parser accepts QString, so this ceiling is explicitly measured in
// UTF-16 code units. A future byte-provider must enforce its byte ceiling
// before decoding and may then call this parser.
inline constexpr int maxDocumentCodeUnits = 65536;
inline constexpr int maxSourceDocuments = 4096;
inline constexpr int maxVisibleEntries = 4096;
inline constexpr int maxDiagnostics = 256;

// Per-entry field ceilings (QString lengths are UTF-16 code units).
inline constexpr int maxEntryIdLength = 256;
inline constexpr int maxNameLength = 256;
inline constexpr int maxGenericNameLength = 256;
inline constexpr int maxCommentLength = 512;
inline constexpr int maxIconNameLength = 128;
inline constexpr int maxCategories = 16;
inline constexpr int maxCategoryLength = 64;
inline constexpr int maxKeywords = 16;
inline constexpr int maxKeywordLength = 64;
inline constexpr int maxActions = 8;
inline constexpr int maxActionIdLength = 64;
inline constexpr int maxActionNameLength = 256;

// Query and collection bounds for the interaction model.
inline constexpr int maxQueryLength = 128;
inline constexpr int maxPinnedEntries = 16;
inline constexpr int maxRecentEntries = 8;

inline bool isValidEntryId(const QString &entryId)
{
  return !entryId.isEmpty() && entryId.size() <= maxEntryIdLength;
}

inline QString boundedDiagnosticEntryId(const QString &entryId)
{
  return entryId.left(maxEntryIdLength);
}

} // namespace QindaQt::ShellLauncher::Bounds
