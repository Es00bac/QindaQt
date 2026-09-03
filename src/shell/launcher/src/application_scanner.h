// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/application_catalog.h"
#include "qindaqt/shell_launcher/launcher_types.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

class QFileSystemWatcher;
class QTimer;

namespace QindaQt::Shell::Launcher {

using QindaQt::ShellLauncher::ApplicationCatalog;
using QindaQt::ShellLauncher::CatalogDiagnostic;

// Production installed-application provider for the launcher. Scans the
// `applications/` tree of each injected data root (XDG desktop-entry id rules:
// subdirectory separators become `-`), feeds bounded documents to the L0
// catalog builder in root-precedence order, and republishes on debounced
// QFileSystemWatcher notifications.
//
// AGENT-CONTRACT: Roots come only from the composition root (the XDG
// data-home/data-dirs list is resolved there). This class never reads
// environment variables, never follows roots it was not given, and never
// executes document content. Raw document text is retained (bounded) solely
// so the execution adapter can extract its own execution keys from the exact
// document the catalog validated.
//
// Generation fencing: every completed rebuild publishes a new generation.
// Consumers compare the generation they acted on against generation() and
// discard stale results; the scanner itself is synchronous, so fencing
// protects consumers that react asynchronously to catalogChanged.
class ApplicationScanner final : public QObject
{
  Q_OBJECT

public:
  explicit ApplicationScanner(QStringList dataRoots, QObject *parent = nullptr);
  ~ApplicationScanner() override;

  // Performs the initial synchronous rebuild and installs watchers. Fails
  // only on a contract violation (empty root list). A syscall-confirmed
  // missing root is normal; unreadable roots and indeterminate root metadata
  // degrade the published catalog instead of failing startup.
  [[nodiscard]] bool start(QString *error = nullptr);
  void stop();
  [[nodiscard]] bool started() const noexcept { return m_started; }

  // Present after the first completed rebuild.
  [[nodiscard]] const std::optional<ApplicationCatalog> &catalog() const noexcept
  {
    return m_catalog;
  }
  // Scanner-level degradation (unreadable roots/files, oversized files,
  // scan ceilings), separate from the catalog's document diagnostics.
  [[nodiscard]] const QVector<CatalogDiagnostic> &scanDiagnostics() const noexcept
  {
    return m_scanDiagnostics;
  }
  [[nodiscard]] bool scanDiagnosticsTruncated() const noexcept
  {
    return m_scanDiagnosticsTruncated;
  }
  [[nodiscard]] quint64 generation() const noexcept { return m_generation; }

  // Raw text and absolute path of the document that claimed this entry id in
  // the current generation, for the execution adapter. Absent for unknown or
  // out-of-retention ids; execution must fail closed on absence.
  [[nodiscard]] std::optional<QString> documentText(const QString &entryId) const;
  [[nodiscard]] QString documentPath(const QString &entryId) const;

Q_SIGNALS:
  void catalogChanged(quint64 generation);

private:
  struct RetainedDocument {
    QString text;
    QString absolutePath;
  };

  void rebuild();
  void scheduleRebuild();
  void scanRoot(const QString &root, int *remainingFiles, bool *ceilingHit);
  void scanDirectory(const QString &applicationsDir, const QString &canonicalRoot,
                     const QString &relativePrefix,
                     int *remainingFiles, bool *ceilingHit,
                     QStringList *watchedDirectories,
                     QSet<QString> *visitedDirectories);
  void addScanDiagnostic(const QString &sourceId, const QString &message);

  QStringList m_roots;
  std::unique_ptr<QFileSystemWatcher> m_watcher;
  QTimer *m_debounce = nullptr;
  std::optional<ApplicationCatalog> m_catalog;
  QVector<CatalogDiagnostic> m_scanDiagnostics;
  QHash<QString, RetainedDocument> m_documents;
  // Valid only during rebuild(); scanDirectory appends through this pointer.
  QVector<QindaQt::ShellLauncher::SourceDocument> *m_pendingDocuments = nullptr;
  quint64 m_generation = 0;
  bool m_started = false;
  bool m_scanDiagnosticsTruncated = false;
};

} // namespace QindaQt::Shell::Launcher
