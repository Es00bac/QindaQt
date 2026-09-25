// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"

#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: bounded Steam library discovery for ADR-0231. VDF/ACF
// parsing itself is NOT re-implemented here: the readers are the compositor's
// public, byte-bounded parsers (QindaQt::Compositor, ADR-0230), linked
// through QindaQt::CompositorShellActions. This adapter owns only the
// filesystem walk -- injected candidate roots, fixed-name file reads under
// the same byte caps, and a hard ceiling on roots, manifests and titles.
// Steam is optional: absent roots, malformed indexes and hostile manifests
// degrade to fewer games plus a warning string, never to a failure.

struct SteamDiscovery final {
  QVector<Game> games;
  // Human-readable degradation notes (e.g. "libraryfolders.vdf malformed in
  // /x"), capped; surfaced in the status bar, never a dialog.
  QStringList warnings;
};

inline constexpr int kMaxSteamCandidateRoots = 8;
inline constexpr int kMaxSteamManifestsPerRoot = 512;
inline constexpr qint64 kMaxSteamFileBytes = qint64(1024) * 1024;

// candidateRoots are directories that may be Steam install roots (each
// holding steamapps/). Production resolves the conventional list
// (~/.steam/root, ~/.local/share/Steam, the flatpak path); tests inject
// fixtures. Roots are tried in order, duplicates collapse, and the total is
// capped. The install root of each candidate also anchors cover-art lookup
// (appcache/librarycache/<appid>/), which is per-install, not per-library.
[[nodiscard]] SteamDiscovery scanSteamLibraries(const QStringList &candidateRoots);

// AGENT-NOTE: Proton discovery moved to proton_catalog.h (ADR-0275). The
// former ProtonInstall/discoverProtonInstalls pair here matched builds by
// script path and let an unpinned entry take "any discovered Proton"; the
// catalog identifies builds by directory name and never substitutes.

} // namespace QindaQt::QindaLutris
