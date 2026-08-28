// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_types.h"

#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::ShellLauncher {

// An immutable, deterministically ordered set of validated visible entries
// plus bounded diagnostics. Build is a total function of its input documents:
// hostile input degrades the result instead of failing the call.
class ApplicationCatalog {
public:
  // AGENT-CONTRACT: sourceId is the desktop-entry identity. The first
  // document for an id wins; later duplicates and hidden documents never
  // enter entries(). Hidden entries are skipped without a diagnostic because
  // NoDisplay/Hidden are normal producer hints, while parse failures,
  // duplicates, and bound hits are real degradation signals.
  static ApplicationCatalog build(const QVector<SourceDocument> &documents);

  // Case-insensitive display-name order, then case-insensitive id, then
  // exact id; consumers may rely on this order being stable for identical
  // input.
  const QVector<ApplicationEntry> &entries() const { return m_entries; }
  const QVector<CatalogDiagnostic> &diagnostics() const { return m_diagnostics; }
  bool diagnosticsTruncated() const { return m_diagnosticsTruncated; }

  std::optional<ApplicationEntry> entry(const QString &entryId) const;

  // Resolves a launch request against this catalog. The result carries only
  // identity and display values; it contains no command line and no way to
  // execute anything.
  LaunchIntentResult makeLaunchIntent(const QString &entryId,
                                      const QString &actionId = {}) const;

private:
  QVector<ApplicationEntry> m_entries;
  QVector<CatalogDiagnostic> m_diagnostics;
  bool m_diagnosticsTruncated = false;

  void addDiagnostic(const CatalogDiagnostic &diagnostic);
};

} // namespace QindaQt::ShellLauncher
