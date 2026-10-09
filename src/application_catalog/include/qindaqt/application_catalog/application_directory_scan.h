// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/application_catalog.h"
#include "qindaqt/shell_launcher/launcher_types.h"

#include <QString>
#include <QVector>

namespace QindaQt::ApplicationCatalog {

using QindaQt::ShellLauncher::ApplicationEntry;
using QindaQt::ShellLauncher::ApplicationVisibility;
using QindaQt::ShellLauncher::CatalogDiagnostic;

using QindaQt::ShellLauncher::ManagedApplicationRegistration;

// One installed, catalog-validated application plus the raw document the
// scanner retained for it. The document text is the exact input that produced
// the entry, so execution-key extraction always plans from validated bytes
// for native entries. Managed entries have no document or executable path;
// their typed binding routes only through the admitted runtime owner.
// (the same contract the shell launcher's execution adapter relies on).
struct ScannedApplication final
{
    ApplicationEntry entry;
    QString desktopFilePath;
    QString documentText;

    friend bool operator==(const ScannedApplication &,
                           const ScannedApplication &) = default;
};

struct DirectoryScan final
{
    // Validated, id-unique entries in catalog display order. The default
    // scan includes menu entries only; the explicit visibility overload can
    // also retain NoDisplay MIME handlers. First root wins each identity.
    QVector<ScannedApplication> applications;
    // Scanner-level degradation (unreadable roots/files, ceilings), separate
    // from the catalog's own document diagnostics.
    QVector<CatalogDiagnostic> diagnostics;
    bool diagnosticsTruncated = false;

    [[nodiscard]] const ScannedApplication *application(
        const QString &entryId) const;
};

// Synchronously scans the `applications/` tree of each injected data root
// (XDG desktop-entry id rules: subdirectory separators become `-`), first
// root wins per entry id, and validates documents through the shared L0
// parser. Pure-Qt filesystem adapter.
//
// AGENT-CONTRACT: Roots come only from the caller (composition roots resolve
// the XDG data-home/data-dirs list). This function never reads environment
// variables, never follows roots it was not given, and never executes
// document content. Ceilings are the shared launcher bounds; hitting one
// degrades the result with a diagnostic instead of failing.
[[nodiscard]] DirectoryScan scanApplicationDirectories(
    const QStringList &dataRoots, QString *error = nullptr);

// Same injected-root, synchronous scan and bounded validation as above.
// IncludeNoDisplay is for MIME-handler discovery, never a change to menu
// consumers. Hidden/deleted and malformed higher-root entries still mask
// lower-root copies. The original overload keeps its source/link signature;
// these static-library APIs require recompilation after public value changes.
[[nodiscard]] DirectoryScan scanApplicationDirectories(
    const QStringList &dataRoots, ApplicationVisibility visibility,
    QString *error = nullptr);

// Adds a complete caller-supplied managed registration snapshot through the
// same pure catalog merge as the launcher. The caller owns authentication and
// recipe/materialization verification; scanning itself grants no provenance.
// Any indeterminate filesystem inspection withholds managed rows, because an
// unreadable higher-precedence override cannot be proved absent. Native rows
// remain usable. Returned values own their data and borrow no source objects.
[[nodiscard]] DirectoryScan scanApplicationDirectories(
    const QStringList &dataRoots,
    const QVector<ManagedApplicationRegistration> &registrations,
    ApplicationVisibility visibility = ApplicationVisibility::MenuEntries,
    QString *error = nullptr);

} // namespace QindaQt::ApplicationCatalog
