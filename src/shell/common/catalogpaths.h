// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::Shell {

// Returns low-to-high precedence directories. Explicit/env paths are isolated.
// An empty sourcePath contributes nothing.
[[nodiscard]] QStringList resolveProfileCatalogDirectories(const QString &explicitPath,
                                                           const QString &sourcePath);
// Returns the source catalog directory only for the genuine build-tree
// executable; any other caller receives an empty path.
[[nodiscard]] QString buildTreeSourceDirectory(const char *sourcePath,
                                               const char *buildExecutablePath);
// AGENT-CONTRACT (ADR-0393, docs/wiki/shell/layout-profiles.md "Catalog
// directories"): the shell's
// theme, applet-manifest and applet-policy catalogs merge EVERY XDG data
// directory. These return the directories (or files) to merge in LOW-to-HIGH
// precedence, so the catalog loader lets a later entry override an earlier one
// by id: the source tree only for a catalog the caller names (lowest), then the
// system roots in reverse XDG_DATA_DIRS order, then XDG_DATA_HOME last, the
// way Settings and the profile catalog already merge themes and profiles.
// An explicit path or an environment override is the whole answer and is never
// merged with anything (isolated, as for profiles). Nonexistent directories and
// files are omitted. Never use QStandardPaths::locate for a catalog: it
// answers with the FIRST match, so one stale user directory hid the whole
// installed set and the saved theme was rejected at every login.
[[nodiscard]] QStringList resolveCatalogDataDirectories(const QString &explicitPath,
                                                        const char *environmentName,
                                                        const char *sourcePath,
                                                        const QString &installedSuffix);
[[nodiscard]] QStringList resolveCatalogDataFiles(const QString &explicitPath,
                                                  const char *environmentName,
                                                  const char *sourcePath,
                                                  const QString &installedSuffix);

// First-match legacy resolvers, kept for the preview tool, which presents one
// catalog directory by design. The production shell must use the merging
// resolvers above.
[[nodiscard]] QString resolveCatalogDataDirectory(const QString &explicitPath,
                                                  const char *environmentName,
                                                  const char *sourcePath,
                                                  const QString &installedSuffix);
[[nodiscard]] QString resolveCatalogDataFile(const QString &explicitPath,
                                             const char *environmentName,
                                             const char *sourcePath,
                                             const QString &installedSuffix);

} // namespace QindaQt::Shell
