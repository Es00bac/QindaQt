// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compat_db.h"
#include "launcher_install_job.h"
#include "proton_catalog.h"
#include "store_recipes.h"
#include "title_record.h"

#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: how a finished install or an adopted prefix becomes a
// TitleRecord (ADR-0275 sections 1-3). Pure except appendTitle, so every
// rule is testable without a job, a network or a real prefix.
//
// Pin choice for a NEW title, in order:
//   1. the database's recommendation for this title (recommendedBuildFor),
//      when that build is installed and pinnable;
//   2. otherwise the effective default (chooseDefaultBuild with the user's /
//      database's preferred name);
//   3. otherwise nothing -- the caller refuses the install with a sentence.
// The record stores the chosen build's NAME and VERSION; nothing ever moves
// it afterwards.

[[nodiscard]] GameStore gameStoreForRecipe(const QString &recipeId);

// Database advice for a store launcher (by its first launcher executable
// name and display name) or for a user-titled game (by title and program).
[[nodiscard]] std::optional<CompatAdvice> adviceForRecipe(const CompatDatabase *database,
                                                          const StoreRecipe &recipe);
[[nodiscard]] std::optional<CompatAdvice> adviceForGame(const CompatDatabase *database,
                                                        const QString &title,
                                                        const QString &executable);

[[nodiscard]] std::optional<ProtonBuild> chooseBuildForNewTitle(
    const QVector<ProtonBuild> &builds, const QString &preferredName,
    const CompatDatabase *database, const std::optional<CompatAdvice> &advice);

// For adopting an existing prefix: the pinnable catalog build whose version
// text equals the prefix's own `version` file (what the prefix last ran on),
// so adoption keeps a working setup exactly as it was.
[[nodiscard]] std::optional<ProtonBuild> buildMatchingPrefix(const QString &prefixPath,
                                                            const QVector<ProtonBuild> &builds);

struct NewTitle final {
  QString title;
  TitleKind kind = TitleKind::SetupInstalled;
  GameStore store = GameStore::None;
  QString storeGameId;
  QString prefixPath;
  QString executable;
  QString umuId;
  QString umuStore;
};

// Builds and validates the record. Database environment is filtered with
// isReservedUmuEnvironmentKey (defence in depth: the database validator
// already allowlists). Empty optional and *why set when invalid.
[[nodiscard]] std::optional<TitleRecord> makeTitleRecord(
    const NewTitle &facts, const ProtonBuild &build, const std::optional<CompatAdvice> &advice,
    const QStringList &takenIds, const QString &today, QString *why);

// Reads titles-v1.json, appends `record` (refusing a duplicate prefix or
// executable), writes atomically. A document the store refuses is never
// rewritten: returns false with a plain sentence in *error.
[[nodiscard]] bool appendTitle(const QString &configRoot, const TitleRecord &record,
                               QString *error);

} // namespace QindaQt::QindaLutris
