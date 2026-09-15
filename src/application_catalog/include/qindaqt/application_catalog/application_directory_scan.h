// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/application_catalog.h"
#include "qindaqt/shell_launcher/launcher_types.h"

#include <QString>
#include <QVector>

namespace QindaQt::ApplicationCatalog {

using QindaQt::ShellLauncher::ApplicationEntry;
using QindaQt::ShellLauncher::CatalogDiagnostic;

// One installed, catalog-validated application plus the raw document the
// scanner retained for it. The document text is the exact input that produced
// the entry, so execution-key extraction always plans from validated bytes
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
    // Visible entries only, id-unique, in root-precedence order.
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

} // namespace QindaQt::ApplicationCatalog
