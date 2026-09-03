// SPDX-License-Identifier: LGPL-3.0-or-later
#include "application_scanner.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

namespace QindaQt::Shell::Launcher {
namespace {

using QindaQt::ShellLauncher::Bounds::maxDiagnostics;
using QindaQt::ShellLauncher::Bounds::maxDocumentCodeUnits;
using QindaQt::ShellLauncher::Bounds::maxSourceDocuments;
using QindaQt::ShellLauncher::DiagnosticKind;
using QindaQt::ShellLauncher::SourceDocument;

// A desktop-entry document is bounded in UTF-16 code units by the L0 parser;
// the byte ceiling is the worst-case UTF-8 encoding of that limit, enforced
// before decoding so a hostile file cannot force an oversized allocation.
inline constexpr qint64 maxDesktopFileBytes = 4LL * maxDocumentCodeUnits;
inline constexpr int debounceMilliseconds = 200;

bool isWithinRoot(const QString &canonicalRoot, const QString &canonicalPath)
{
  if (canonicalRoot.isEmpty() || canonicalPath.isEmpty())
    return false;
  return canonicalPath == canonicalRoot
      || canonicalPath.startsWith(canonicalRoot + QDir::separator());
}

QString entryIdFor(const QString &relativePath)
{
  QString id = relativePath;
  if (id.endsWith(QLatin1String(".desktop")))
    id.chop(8);
  id.replace(QLatin1Char('/'), QLatin1Char('-'));
  return id;
}

} // namespace

ApplicationScanner::ApplicationScanner(QStringList dataRoots, QObject *parent)
    : QObject(parent), m_roots(std::move(dataRoots))
{
}

ApplicationScanner::~ApplicationScanner() = default;

bool ApplicationScanner::start(QString *error)
{
  if (m_roots.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("application scanner requires at least one data root");
    }
    return false;
  }
  if (m_started)
    return true;

  m_watcher = std::make_unique<QFileSystemWatcher>(this);
  m_debounce = new QTimer(this);
  m_debounce->setSingleShot(true);
  m_debounce->setInterval(debounceMilliseconds);
  connect(m_debounce, &QTimer::timeout, this, &ApplicationScanner::rebuild);
  connect(m_watcher.get(), &QFileSystemWatcher::directoryChanged,
          this, [this](const QString &) { scheduleRebuild(); });
  connect(m_watcher.get(), &QFileSystemWatcher::fileChanged,
          this, [this](const QString &) { scheduleRebuild(); });

  m_started = true;
  rebuild();
  return true;
}

void ApplicationScanner::stop()
{
  if (!m_started)
    return;
  m_started = false;
  m_debounce->stop();
  m_watcher.reset();
}

void ApplicationScanner::scheduleRebuild()
{
  if (m_started)
    m_debounce->start();
}

void ApplicationScanner::addScanDiagnostic(const QString &sourceId, const QString &message)
{
  if (m_scanDiagnostics.size() >= maxDiagnostics) {
    m_scanDiagnosticsTruncated = true;
    return;
  }
  m_scanDiagnostics.append(CatalogDiagnostic {
      DiagnosticKind::InvalidDocument, sourceId, message });
}

void ApplicationScanner::scanDirectory(const QString &directoryPath,
                                       const QString &canonicalRoot,
                                       const QString &relativePrefix,
                                       int *remainingFiles, bool *ceilingHit,
                                       QStringList *watchedDirectories,
                                       QSet<QString> *visitedDirectories)
{
  const QFileInfo directoryInfo(directoryPath);
  const QString canonicalDirectory = directoryInfo.canonicalFilePath();
  if (!directoryInfo.isDir()
      || !isWithinRoot(canonicalRoot, canonicalDirectory)) {
    addScanDiagnostic(relativePrefix.isEmpty() ? directoryPath : relativePrefix,
                      QStringLiteral("applications directory escapes its data root"));
    return;
  }
  if (visitedDirectories->contains(canonicalDirectory)) {
    addScanDiagnostic(relativePrefix.isEmpty() ? directoryPath : relativePrefix,
                      QStringLiteral("applications directory contains a link cycle"));
    return;
  }
  visitedDirectories->insert(canonicalDirectory);

  QDir directory(canonicalDirectory);
  if (!directory.isReadable()) {
    addScanDiagnostic(relativePrefix.isEmpty() ? directoryPath : relativePrefix,
                      QStringLiteral("applications directory is not readable"));
    return;
  }
  watchedDirectories->append(canonicalDirectory);

  // Deterministic scan order: entry-name sorted, files before recursion into
  // sorted subdirectories, so identical trees always build identical input.
  // AGENT-GUARD: Include QDir::System so hostile non-regular nodes and dangling
  // symlinks become diagnostics. Canonical containment is checked before any
  // recursion or file open; link cycles are fenced by visitedDirectories.
  const QFileInfoList entries = directory.entryInfoList(
      QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot,
      QDir::Name | QDir::IgnoreCase);
  for (const QFileInfo &entry : entries) {
    if (*ceilingHit)
      return;
    const QString relative = relativePrefix.isEmpty()
        ? entry.fileName()
        : relativePrefix + QLatin1Char('/') + entry.fileName();
    if (entry.isDir()) {
      const QString canonicalEntry = entry.canonicalFilePath();
      if (!isWithinRoot(canonicalRoot, canonicalEntry)) {
        addScanDiagnostic(relative,
                          QStringLiteral("applications directory escapes its data root"));
        continue;
      }
      scanDirectory(canonicalEntry, canonicalRoot, relative, remainingFiles,
                    ceilingHit, watchedDirectories, visitedDirectories);
      continue;
    }
    if (!entry.fileName().endsWith(QLatin1String(".desktop")))
      continue;
    if (*remainingFiles <= 0) {
      *ceilingHit = true;
      addScanDiagnostic(entryIdFor(relative),
                        QStringLiteral("scanned desktop file ceiling reached"));
      return;
    }
    --*remainingFiles;

    const QString sourceId = entryIdFor(relative);
    const QString canonicalEntry = entry.canonicalFilePath();
    if (!entry.isFile() || !isWithinRoot(canonicalRoot, canonicalEntry)) {
      addScanDiagnostic(sourceId,
                        QStringLiteral("desktop entry is not a contained regular file"));
      continue;
    }
    const QFileInfo canonicalInfo(canonicalEntry);
    if (canonicalInfo.size() > maxDesktopFileBytes) {
      addScanDiagnostic(sourceId,
                        QStringLiteral("desktop file exceeds the byte ceiling"));
      continue;
    }
    // AGENT-NOTE: Directories alone do not report content edits on every
    // backend, so each desktop file is watched as well; the file ceiling
    // keeps the watch set bounded.
    watchedDirectories->append(canonicalEntry);
    QFile file(canonicalEntry);
    if (!file.open(QIODevice::ReadOnly)) {
      addScanDiagnostic(sourceId, QStringLiteral("desktop file is not readable"));
      continue;
    }
    // AGENT-GUARD: Never use readAll() here. A regular file can grow after its
    // metadata check; one capped read keeps allocation and latency bounded.
    const QByteArray bytes = file.read(maxDesktopFileBytes + 1);
    if (file.error() != QFileDevice::NoError) {
      addScanDiagnostic(sourceId, QStringLiteral("desktop file could not be read"));
      continue;
    }
    if (bytes.size() > maxDesktopFileBytes) {
      addScanDiagnostic(sourceId,
                        QStringLiteral("desktop file exceeds the byte ceiling"));
      continue;
    }
    const QString text = QString::fromUtf8(bytes);
    if (text.size() > maxDocumentCodeUnits) {
      addScanDiagnostic(sourceId,
                        QStringLiteral("desktop file exceeds the document ceiling"));
      continue;
    }
    // AGENT-GUARD: Only the first (highest-precedence) document per id is
    // retained; the catalog claims ids in the same order, so execution always
    // sees exactly the document that won the catalog.
    if (!m_documents.contains(sourceId)) {
      m_documents.insert(sourceId, RetainedDocument { text, canonicalEntry });
    }
    m_pendingDocuments->append(SourceDocument { sourceId, text });
  }
}

void ApplicationScanner::scanRoot(const QString &root, int *remainingFiles,
                                  bool *ceilingHit)
{
  const QFileInfo rootInfo(root);
  const QString canonicalRoot = rootInfo.canonicalFilePath();
  const QString applicationsDir = QDir(root).filePath(QStringLiteral("applications"));
  if (!QFileInfo::exists(applicationsDir))
    return; // A root without an applications tree is normal, not degradation.
  if (canonicalRoot.isEmpty()) {
    addScanDiagnostic(root, QStringLiteral("data root cannot be canonicalized"));
    return;
  }
  QStringList watched;
  QSet<QString> visitedDirectories;
  scanDirectory(applicationsDir, canonicalRoot, QString(), remainingFiles,
                ceilingHit, &watched, &visitedDirectories);
  if (m_watcher && !watched.isEmpty())
    m_watcher->addPaths(watched);
}

void ApplicationScanner::rebuild()
{
  if (!m_started)
    return;

  m_documents.clear();
  m_scanDiagnostics.clear();
  m_scanDiagnosticsTruncated = false;

  QVector<SourceDocument> documents;
  documents.reserve(64);
  m_pendingDocuments = &documents;

  int remainingFiles = maxSourceDocuments;
  bool ceilingHit = false;
  if (m_watcher) {
    if (!m_watcher->directories().isEmpty())
      m_watcher->removePaths(m_watcher->directories());
    if (!m_watcher->files().isEmpty())
      m_watcher->removePaths(m_watcher->files());
  }
  for (const QString &root : std::as_const(m_roots)) {
    if (ceilingHit)
      break;
    scanRoot(root, &remainingFiles, &ceilingHit);
  }
  m_pendingDocuments = nullptr;

  m_catalog = ApplicationCatalog::build(documents);
  // AGENT-GUARD: The generation increments on every completed rebuild, even
  // when the resulting catalog compares equal; consumers fence on it, so a
  // rebuild must never silently reuse a retired generation.
  ++m_generation;
  Q_EMIT catalogChanged(m_generation);
}

std::optional<QString> ApplicationScanner::documentText(const QString &entryId) const
{
  const auto iterator = m_documents.constFind(entryId);
  if (iterator == m_documents.constEnd())
    return std::nullopt;
  return iterator->text;
}

QString ApplicationScanner::documentPath(const QString &entryId) const
{
  const auto iterator = m_documents.constFind(entryId);
  if (iterator == m_documents.constEnd())
    return {};
  return iterator->absolutePath;
}

} // namespace QindaQt::Shell::Launcher
