// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/application_catalog/application_directory_scan.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <sys/stat.h>

namespace QindaQt::ApplicationCatalog {
namespace {

using QindaQt::ShellLauncher::ApplicationCatalog;
using QindaQt::ShellLauncher::Bounds::maxDiagnostics;
using QindaQt::ShellLauncher::Bounds::maxDocumentCodeUnits;
using QindaQt::ShellLauncher::Bounds::maxSourceDocuments;
using QindaQt::ShellLauncher::DiagnosticKind;
using QindaQt::ShellLauncher::SourceDocument;

// A desktop-entry document is bounded in UTF-16 code units by the L0 parser;
// the byte ceiling is the worst-case UTF-8 encoding of that limit, enforced
// before decoding so a hostile file cannot force an oversized allocation.
inline constexpr qint64 maxDesktopFileBytes = 4LL * maxDocumentCodeUnits;

enum class PathPresence {
    Present,
    Missing,
    Indeterminate,
};

PathPresence pathPresence(const QString &path)
{
    struct stat pathStatus {};
    const QByteArray nativePath = QFile::encodeName(path);
    if (::lstat(nativePath.constData(), &pathStatus) == 0) {
        return PathPresence::Present;
    }
    return errno == ENOENT ? PathPresence::Missing : PathPresence::Indeterminate;
}

bool isWithinRoot(const QString &canonicalRoot, const QString &canonicalPath)
{
    if (canonicalRoot.isEmpty() || canonicalPath.isEmpty()) {
        return false;
    }
    return canonicalPath == canonicalRoot
        || canonicalPath.startsWith(canonicalRoot + QDir::separator());
}

QString entryIdFor(const QString &relativePath)
{
    QString id = relativePath;
    if (id.endsWith(QLatin1String(".desktop"))) {
        id.chop(8);
    }
    id.replace(QLatin1Char('/'), QLatin1Char('-'));
    return id;
}

struct WalkContext final
{
    QVector<SourceDocument> *documents = nullptr;
    QHash<QString, QPair<QString, QString>> *documentsBySourceId = nullptr;
    QVector<CatalogDiagnostic> *diagnostics = nullptr;
    bool *diagnosticsTruncated = nullptr;
};

void addDiagnostic(WalkContext &context, const QString &sourceId,
                   const QString &message)
{
    if (context.diagnostics->size() >= maxDiagnostics) {
        *context.diagnosticsTruncated = true;
        return;
    }
    context.diagnostics->append(
        CatalogDiagnostic{DiagnosticKind::InvalidDocument, sourceId, message});
}

void scanDirectory(WalkContext &context, const QString &directoryPath,
                   const QString &canonicalRoot,
                   const QString &relativePrefix, int *remainingFiles,
                   bool *ceilingHit, QSet<QString> *visitedDirectories)
{
    const QFileInfo directoryInfo(directoryPath);
    const QString canonicalDirectory = directoryInfo.canonicalFilePath();
    if (!directoryInfo.isDir() || !isWithinRoot(canonicalRoot, canonicalDirectory)) {
        addDiagnostic(context,
                      relativePrefix.isEmpty() ? directoryPath : relativePrefix,
                      QStringLiteral("applications directory escapes its data root"));
        return;
    }
    if (visitedDirectories->contains(canonicalDirectory)) {
        addDiagnostic(context,
                      relativePrefix.isEmpty() ? directoryPath : relativePrefix,
                      QStringLiteral("applications directory contains a link cycle"));
        return;
    }
    visitedDirectories->insert(canonicalDirectory);

    QDir directory(canonicalDirectory);
    if (!directory.isReadable()) {
        addDiagnostic(context,
                      relativePrefix.isEmpty() ? directoryPath : relativePrefix,
                      QStringLiteral("applications directory is not readable"));
        return;
    }

    // AGENT-GUARD: Deterministic scan order (entry-name sorted, files before
    // recursion into sorted subdirectories) with canonical containment checked
    // before any recursion or file open; link cycles are fenced by
    // visitedDirectories. QDir::System surfaces hostile non-regular nodes and
    // dangling symlinks as diagnostics instead of skipping them silently.
    const QFileInfoList entries = directory.entryInfoList(
        QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        if (*ceilingHit) {
            return;
        }
        const QString relative = relativePrefix.isEmpty()
            ? entry.fileName()
            : relativePrefix + QLatin1Char('/') + entry.fileName();
        if (entry.isDir()) {
            const QString canonicalEntry = entry.canonicalFilePath();
            if (!isWithinRoot(canonicalRoot, canonicalEntry)) {
                addDiagnostic(context, relative,
                              QStringLiteral("applications directory escapes its data root"));
                continue;
            }
            scanDirectory(context, canonicalEntry, canonicalRoot, relative,
                          remainingFiles, ceilingHit, visitedDirectories);
            continue;
        }
        if (!entry.fileName().endsWith(QLatin1String(".desktop"))) {
            continue;
        }
        if (*remainingFiles <= 0) {
            *ceilingHit = true;
            addDiagnostic(context, entryIdFor(relative),
                          QStringLiteral("scanned desktop file ceiling reached"));
            return;
        }
        --*remainingFiles;

        const QString sourceId = entryIdFor(relative);
        const QString canonicalEntry = entry.canonicalFilePath();
        if (!entry.isFile() || !isWithinRoot(canonicalRoot, canonicalEntry)) {
            addDiagnostic(context, sourceId,
                          QStringLiteral("desktop entry is not a contained regular file"));
            continue;
        }
        const QFileInfo canonicalInfo(canonicalEntry);
        if (canonicalInfo.size() > maxDesktopFileBytes) {
            addDiagnostic(context, sourceId,
                          QStringLiteral("desktop file exceeds the byte ceiling"));
            continue;
        }
        QFile file(canonicalEntry);
        if (!file.open(QIODevice::ReadOnly)) {
            addDiagnostic(context, sourceId,
                          QStringLiteral("desktop file is not readable"));
            continue;
        }
        // AGENT-GUARD: Never use readAll() here. A regular file can grow after
        // its metadata check; one capped read keeps allocation and latency
        // bounded.
        const QByteArray bytes = file.read(maxDesktopFileBytes + 1);
        if (file.error() != QFileDevice::NoError) {
            addDiagnostic(context, sourceId,
                          QStringLiteral("desktop file could not be read"));
            continue;
        }
        if (bytes.size() > maxDesktopFileBytes) {
            addDiagnostic(context, sourceId,
                          QStringLiteral("desktop file exceeds the byte ceiling"));
            continue;
        }
        const QString text = QString::fromUtf8(bytes);
        if (text.size() > maxDocumentCodeUnits) {
            addDiagnostic(context, sourceId,
                          QStringLiteral("desktop file exceeds the document ceiling"));
            continue;
        }
        // AGENT-GUARD: Only the first (highest-precedence) document per id is
        // retained; the catalog claims ids in the same order, so launch
        // planning always sees exactly the document that won the catalog.
        if (!context.documentsBySourceId->contains(sourceId)) {
            context.documentsBySourceId->insert(sourceId,
                                                {canonicalEntry, text});
        }
        context.documents->append(SourceDocument{sourceId, text});
    }
}

void scanRoot(WalkContext &context, const QString &root, int *remainingFiles,
              bool *ceilingHit)
{
    // AGENT-GUARD: QFileInfo maps both ENOENT and metadata errors such as
    // EACCES to exists()==false. Only a syscall-confirmed ENOENT is normal
    // absence; every other status failure must remain visible as degraded
    // catalog truth.
    const PathPresence rootPresence = pathPresence(root);
    if (rootPresence == PathPresence::Missing) {
        return;
    }
    if (rootPresence == PathPresence::Indeterminate) {
        addDiagnostic(context, root,
                      QStringLiteral("data root status cannot be determined"));
        return;
    }

    const QFileInfo rootInfo(root);
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    if (!rootInfo.isDir() || canonicalRoot.isEmpty()) {
        addDiagnostic(context, root,
                      QStringLiteral("data root cannot be canonicalized"));
        return;
    }
    if (!rootInfo.isReadable()) {
        addDiagnostic(context, root,
                      QStringLiteral("data root is not readable"));
        return;
    }

    const QString applicationsDir =
        QDir(canonicalRoot).filePath(QStringLiteral("applications"));
    const PathPresence applicationsPresence = pathPresence(applicationsDir);
    if (applicationsPresence == PathPresence::Missing) {
        return; // A readable root without an applications tree is normal.
    }
    if (applicationsPresence == PathPresence::Indeterminate) {
        addDiagnostic(context, applicationsDir,
                      QStringLiteral("applications directory status cannot be determined"));
        return;
    }

    const QFileInfo applicationsInfo(applicationsDir);
    if (applicationsInfo.isSymLink()
        && applicationsInfo.canonicalFilePath().isEmpty()) {
        addDiagnostic(context, applicationsDir,
                      QStringLiteral("applications directory link is dangling"));
        return;
    }
    QSet<QString> visitedDirectories;
    scanDirectory(context, applicationsDir, canonicalRoot, QString(),
                  remainingFiles, ceilingHit, &visitedDirectories);
}

} // namespace

const ScannedApplication *DirectoryScan::application(
    const QString &entryId) const
{
    const auto match = std::find_if(applications.cbegin(), applications.cend(),
                                    [&entryId](const ScannedApplication &candidate) {
                                        return candidate.entry.id == entryId;
                                    });
    return match == applications.cend() ? nullptr : &*match;
}

DirectoryScan scanApplicationDirectories(const QStringList &dataRoots,
                                         QString *error)
{
    return scanApplicationDirectories(dataRoots, ApplicationVisibility::MenuEntries, error);
}

DirectoryScan scanApplicationDirectories(const QStringList &dataRoots,
                                         ApplicationVisibility visibility,
                                         QString *error)
{
    if (dataRoots.isEmpty()) {
        if (error) {
            *error = QStringLiteral("application scan requires at least one data root");
        }
        return {};
    }

    DirectoryScan scan;
    QVector<SourceDocument> documents;
    documents.reserve(64);
    // sourceId -> {absolute path, raw document text} of the retained winner.
    QHash<QString, QPair<QString, QString>> documentsBySourceId;
    documentsBySourceId.reserve(64);
    WalkContext context{&documents, &documentsBySourceId,
                        &scan.diagnostics, &scan.diagnosticsTruncated};

    int remainingFiles = maxSourceDocuments;
    bool ceilingHit = false;
    for (const QString &root : dataRoots) {
        if (ceilingHit) {
            break;
        }
        scanRoot(context, root, &remainingFiles, &ceilingHit);
    }

    const auto catalog = ApplicationCatalog::build(documents, visibility);
    scan.applications.reserve(catalog.entries().size());
    for (const auto &entry : catalog.entries()) {
        const auto document = documentsBySourceId.constFind(entry.id);
        if (document == documentsBySourceId.cend()) {
            continue;
        }
        scan.applications.append(
            ScannedApplication{entry, document->first, document->second});
    }
    scan.diagnostics.append(catalog.diagnostics());
    scan.diagnosticsTruncated = scan.diagnosticsTruncated
        || catalog.diagnosticsTruncated();
    return scan;
}

} // namespace QindaQt::ApplicationCatalog
