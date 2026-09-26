// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compat_db.h"

#include <QVariantMap>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the verdict card (ADR-0275 section 7) as plain values for
// QML. A card is advice, never a gate -- except `canRun == false` (anti-cheat
// that refuses Linux), which the UI uses to disable Install.
//
// Keys: found (bool), verdict ("works" | "fixes" | "unknown" | "blocked"),
// verdictText, canRun (bool), antiCheat, antiCheatNotes, protondb
// (community rating text, empty when unknown), steamDeck, steamDeckNotes,
// fixes (list of plain lines: winetricks components and settings the app
// applies), avoid (list of {build, reason}), notes, links, sources (the
// attribution line, including ProtonDB's ODbL credit when a tier is shown).
[[nodiscard]] QVariantMap adviceToVariant(const std::optional<CompatAdvice> &advice);

// Keys for a library game map (Library.selectedGame): title, and the Steam
// appid from a "steam/<appid>" id, and the program name from installPath.
[[nodiscard]] GameKeys keysForLibraryGame(const QVariantMap &game);

} // namespace QindaQt::QindaLutris
