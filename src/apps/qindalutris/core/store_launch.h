// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "launch_planner.h"
#include "title_record.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: Epic, GOG and Amazon games start through their store
// client (ADR-0275 section 8) -- the client knows the game's own launch
// task and, for Epic, fetches the online sign-in token the game expects --
// but the client never chooses Wine. It is told `--no-wine --wrapper <w>`,
// where <w> is exactly the part of the ordinary umu plan in front of the
// game's program: umu-run, plus gamescope / gamemoderun when the user asked
// for them. So a store game gets the same pinned build, the same five umu
// variables, the same reserved-key removal and the same refusals as every
// other title (planUmuLaunch runs first and its refusals are returned
// unchanged). The client inherits the plan's environment and passes it to
// the wrapper; StoreClientSet::environment is added so only QindaLutris's
// own client configuration is ever read.
//
//   Epic   legendary launch <app> --no-wine --wrapper <w> --skip-version-check
//   GOG    gogdl launch <installDir> <id> --platform windows --no-wine --wrapper <w>
//   Amazon nile launch <id> --no-wine --wrapper <w>
//
// AGENT-GUARD: the store game id is checked against isSafeStoreGameId
// before it becomes argv (a value starting with '-' would be read as an
// option by the client's argparse). A title's own `arguments` are NOT
// passed through for the same reason; they are dropped with a note.
// Updating a game is a separate, explicit action: a launch never updates.

// Egs, Gog and Amazon.
[[nodiscard]] bool launchesThroughStoreClient(GameStore store);

// [A-Za-z0-9][A-Za-z0-9._:-]{0,127}: Epic app names, GOG product ids and
// Amazon product ids ("amzn1.adg.product.<uuid>") all fit.
[[nodiscard]] bool isSafeStoreGameId(const QString &id);

// Quotes tokens so Python's shlex.split (which every client uses on
// --wrapper) returns them unchanged. Pure.
[[nodiscard]] QString joinForShlex(const QStringList &tokens);

// A GOG game's install directory is the folder holding
// goggame-<id>.info; gogdl reads the launch task from it. Walks up from the
// program's folder at most kMaxGogInfoDepth levels. Empty when not found.
inline constexpr int kMaxGogInfoDepth = 6;
[[nodiscard]] QString findGogInstallDirectory(const QString &executable,
                                              const QString &gogId);

// Plans a StoreGame title of a client-launched store (see contract).
// Refusals are one plain sentence.
[[nodiscard]] LaunchPlan planStoreGameLaunch(const TitleRecord &title,
                                             const LaunchOptions &options,
                                             const LaunchToolSet &tools,
                                             const QVector<DisplayTarget> &displays);

} // namespace QindaQt::QindaLutris
