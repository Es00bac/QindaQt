// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the one-click store recipes of ADR-0275 section 4 --
// pure data plus validation. A recipe names a vendor HTTPS installer (always
// on the download allowlist), how to run it, the prefix it gets by default,
// the umu identity to launch it with, and where the launcher lands inside the
// prefix. Recipe ids are persisted by TitleRecord (recipeId); never rename
// one without a store migration.

enum class InstallerKind {
  Exe, // runs as `umu-run <installer> <args...>`
  Msi, // runs as `umu-run msiexec /i <installer> <args...>`
};

struct StoreRecipe final {
  QString id;          // "battlenet", "ea", "ubisoft", ...
  QString displayName; // "Battle.net"
  QUrl installerUrl;
  QString installerFileName;
  InstallerKind installerKind = InstallerKind::Exe;
  // Only flags verified from Lutris install scripts or vendor docs; when
  // empty the vendor installer's own window is shown to the user.
  QStringList installerArguments;
  QString defaultPrefixDirName; // under ~/Games by default
  QString umuId;                // "umu-0" unless umu-database names one
  QString umuStore;             // umu STORE value
  // Windows paths, in preference order; a whole-segment `*` matches one
  // directory level (see expandPrefixCandidate()).
  QStringList launcherExecutableCandidates;
  QString plainNotes;           // shown to the user before installing
  qint64 minimumFreeBytes = 0;  // preflight floor for this launcher

  friend bool operator==(const StoreRecipe &, const StoreRecipe &) = default;
};

// The fixed recipe table, in presentation order.
[[nodiscard]] const QVector<StoreRecipe> &storeRecipes();
[[nodiscard]] std::optional<StoreRecipe> findStoreRecipe(const QString &id);

// umu STORE values accepted by recipes and titles (umu-protonfixes
// `gamefixes-<store>` directories plus "none").
[[nodiscard]] const QStringList &knownUmuStores();

// [a-z0-9][a-z0-9._-]{0,63}, never "." or "..": safe as one directory name.
[[nodiscard]] bool isSafePrefixDirName(const QString &name);

// Human-readable problems; empty means the recipe is valid.
[[nodiscard]] QStringList validateStoreRecipe(const StoreRecipe &recipe);

// What runs inside the prefix (the argv after `umu-run`): the installer and
// its arguments, or `msiexec /i <installer> <arguments...>` for an MSI.
[[nodiscard]] QStringList installerCommand(InstallerKind kind, const QString &installerPath,
                                           const QStringList &arguments);

} // namespace QindaQt::QindaLutris
