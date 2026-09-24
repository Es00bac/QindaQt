// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0269's item actions for MutationController (Duplicate, Make Link,
// Delete Permanently, Put Back, New File, Compress, Extract), split from
// mutation_controller.cpp to keep both files under the source-shape budget.
// Same class, same module: these methods only name destinations and build
// identity-checked requests; every one runs through submit/submitRequests.
#include "mutation_controller.h"

#include "archive_codec.h"
#include "home_trash.h"
#include "local_mutation_backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

// Generous but finite: a folder holding a thousand copies of one name makes
// the action fail instead of spinning.
constexpr int maximumNameAttempts = 1000;

// The first "<stem><suffix>", "<stem> 2<suffix>", "<stem> 3<suffix>", ... in
// `folder` that no entry (a dangling link included) and no name already
// planned by this request occupies; empty when the bound runs out.
[[nodiscard]] QString firstFreePath(const QString &folder, const QString &stem,
                                    const QString &suffix, const QSet<QString> &planned) {
  for (int attempt = 1; attempt <= maximumNameAttempts; ++attempt) {
    const QString name =
        (attempt == 1 ? stem : QStringLiteral("%1 %2").arg(stem).arg(attempt)) + suffix;
    const QString path = QDir(folder).filePath(name);
    const QFileInfo existing(path);
    if (!planned.contains(path) && !existing.exists() && !existing.isSymLink()) {
      return path;
    }
  }
  return {};
}

// "report.txt" -> {"report", ".txt"}; a folder, a dot file or a name without
// an extension keeps its whole name as the stem.
[[nodiscard]] std::pair<QString, QString> stemAndSuffix(const QString &path) {
  const QFileInfo info(path);
  const QString stem = info.completeBaseName();
  if (info.isDir() || stem.isEmpty() || info.suffix().isEmpty()) {
    return {info.fileName(), QString()};
  }
  return {stem, QLatin1Char('.') + info.suffix()};
}

} // namespace

bool MutationController::validName(const QString &name) {
  return !name.isEmpty() && QFile::encodeName(name).size() <= 255 &&
         name != QLatin1String(".") && name != QLatin1String("..") &&
         !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\')) &&
         !name.contains(QChar::Null);
}

QStringList MutationController::rootsFor(const QString &source, const QString &destination) {
  QStringList roots;
  for (const QString &path : {source, destination}) {
    if (!path.isEmpty()) {
      roots.append(QFileInfo(path).absolutePath());
    }
  }
  roots.removeDuplicates();
  return roots;
}

bool MutationController::parseItem(const QVariant &item, QString *path, FileIdentity *identity) {
  const QVariantMap map =
      item.metaType().id() == QMetaType::QVariantMap ? item.toMap() : QVariantMap();
  const std::optional<FileIdentity> parsed = identityFromMap(map);
  *path = map.value(QStringLiteral("path")).toString();
  if (path->isEmpty() || !parsed) {
    fail(MutationError::InvalidRequest,
         QStringLiteral("The selection is stale; refresh and try again"));
    return false;
  }
  *identity = *parsed;
  return true;
}

bool MutationController::duplicateItems(const QVariantList &items) {
  QVector<MutationRequest> requests;
  QSet<QString> planned;
  for (const QVariant &item : items) {
    MutationRequest request;
    FileIdentity identity;
    if (!parseItem(item, &request.sourcePath, &identity)) {
      return false;
    }
    const QString folder = QFileInfo(request.sourcePath).absolutePath();
    const auto [stem, suffix] = stemAndSuffix(request.sourcePath);
    request.destinationPath = firstFreePath(folder, stem + QStringLiteral(" copy"), suffix, planned);
    if (request.destinationPath.isEmpty()) {
      fail(MutationError::AlreadyExists, QStringLiteral("No free name is left for a copy"));
      return false;
    }
    planned.insert(request.destinationPath);
    request.kind = MutationKind::Copy;
    request.declaredRoots = rootsFor(request.sourcePath, request.destinationPath);
    request.expectedSource = identity;
    request.expectedParent = LocalMutationBackend::identityForPath(folder);
    requests.append(std::move(request));
  }
  return submitRequests(MutationKind::Copy, std::move(requests));
}

bool MutationController::makeLinks(const QVariantList &items) {
  QVector<MutationRequest> requests;
  QSet<QString> planned;
  for (const QVariant &item : items) {
    MutationRequest request;
    FileIdentity identity;
    if (!parseItem(item, &request.sourcePath, &identity)) {
      return false;
    }
    const QString folder = QFileInfo(request.sourcePath).absolutePath();
    const auto [stem, suffix] = stemAndSuffix(request.sourcePath);
    request.destinationPath =
        firstFreePath(folder, QStringLiteral("Link to %1").arg(stem), suffix, planned);
    if (request.destinationPath.isEmpty()) {
      fail(MutationError::AlreadyExists, QStringLiteral("No free name is left for a link"));
      return false;
    }
    planned.insert(request.destinationPath);
    request.kind = MutationKind::Link;
    // A sibling name, not an absolute path: the link survives moving the folder.
    request.linkTarget = QFileInfo(request.sourcePath).fileName();
    request.declaredRoots = {folder};
    request.expectedSource = identity;
    request.expectedParent = LocalMutationBackend::identityForPath(folder);
    requests.append(std::move(request));
  }
  return submitRequests(MutationKind::Link, std::move(requests));
}

bool MutationController::deleteItems(const QVariantList &items) {
  QVector<MutationRequest> requests;
  for (const QVariant &item : items) {
    MutationRequest request;
    FileIdentity identity;
    if (!parseItem(item, &request.sourcePath, &identity)) {
      return false;
    }
    request.kind = MutationKind::Delete;
    request.declaredRoots = rootsFor(request.sourcePath);
    request.expectedSource = identity;
    requests.append(std::move(request));
  }
  return submitRequests(MutationKind::Delete, std::move(requests));
}

bool MutationController::putBackItems(const QVariantList &items) {
  QVector<MutationRequest> requests;
  for (const QVariant &item : items) {
    QString payload;
    FileIdentity identity;
    if (!parseItem(item, &payload, &identity)) {
      return false;
    }
    const QString name = QFileInfo(payload).fileName();
    const QString original = HomeTrash::originalPathFor(payload);
    if (original.isEmpty()) {
      fail(MutationError::InvalidRequest,
           QStringLiteral("“%1” has no Trash record to put it back from").arg(name));
      return false;
    }
    const QString folder = QFileInfo(original).absolutePath();
    MutationRequest request;
    request.expectedParent = LocalMutationBackend::identityForPath(folder);
    if (!request.expectedParent) {
      fail(MutationError::Vanished,
           QStringLiteral("The folder “%1” came from no longer exists").arg(name));
      return false;
    }
    // The same request Restore Last builds; HomeTrash::restore re-reads the
    // record and refuses unless it still names this exact path.
    request.kind = MutationKind::Restore;
    request.trashToken = name;
    request.destinationPath = original;
    request.declaredRoots = {folder};
    request.expectedSource = identity;
    requests.append(std::move(request));
  }
  return submitRequests(MutationKind::Restore, std::move(requests));
}

bool MutationController::createFile(const QString &parentPath, const QString &name,
                                    const QString &templatePath) {
  if (!validName(name)) {
    fail(MutationError::InvalidRequest, QStringLiteral("Choose a valid file name"));
    return false;
  }
  MutationRequest request;
  request.destinationPath = QDir(parentPath).filePath(name);
  request.expectedParent = LocalMutationBackend::identityForPath(parentPath);
  if (templatePath.isEmpty()) {
    request.kind = MutationKind::CreateFile;
    request.declaredRoots = {QFileInfo(parentPath).absoluteFilePath()};
    return submit(std::move(request));
  }
  // A template is copied like any other file: its identity is taken now and
  // the backend re-verifies it before copying a single byte.
  request.kind = MutationKind::Copy;
  request.sourcePath = QFileInfo(templatePath).absoluteFilePath();
  request.declaredRoots = rootsFor(request.sourcePath, request.destinationPath);
  request.expectedSource = LocalMutationBackend::identityForPath(request.sourcePath);
  if (!request.expectedSource) {
    fail(MutationError::Vanished, QStringLiteral("The template is no longer available"));
    return false;
  }
  return submit(std::move(request));
}

bool MutationController::compressItems(const QVariantList &items) {
  MutationRequest request;
  request.kind = MutationKind::Compress;
  QSet<QString> names;
  for (const QVariant &item : items) {
    QString source;
    FileIdentity identity;
    if (!parseItem(item, &source, &identity)) {
      return false;
    }
    // Each item is stored under its own name, so two items of one name
    // (search results from different folders) would collide in the archive.
    const QString name = QFileInfo(source).fileName();
    if (names.contains(name)) {
      fail(MutationError::InvalidRequest,
           QStringLiteral("Two selected items are both named “%1”").arg(name));
      return false;
    }
    names.insert(name);
    request.archiveSources.append(source);
    request.archiveSourceIdentities.append(identity);
    request.declaredRoots.append(rootsFor(source));
  }
  if (request.archiveSources.isEmpty()) {
    fail(MutationError::InvalidRequest, QStringLiteral("No items are selected"));
    return false;
  }
  const QFileInfo first(request.archiveSources.constFirst());
  const QString folder = first.absolutePath();
  request.destinationPath = firstFreePath(
      folder, request.archiveSources.size() == 1 ? first.fileName() : QStringLiteral("Archive"),
      QStringLiteral(".zip"), {});
  if (request.destinationPath.isEmpty()) {
    fail(MutationError::AlreadyExists, QStringLiteral("No free name is left for the archive"));
    return false;
  }
  request.declaredRoots.append(folder);
  request.declaredRoots.removeDuplicates();
  request.expectedParent = LocalMutationBackend::identityForPath(folder);
  return submit(std::move(request));
}

bool MutationController::extractItems(const QVariantList &items) {
  QVector<MutationRequest> requests;
  QSet<QString> planned;
  for (const QVariant &item : items) {
    MutationRequest request;
    FileIdentity identity;
    if (!parseItem(item, &request.sourcePath, &identity)) {
      return false;
    }
    const QFileInfo archive(request.sourcePath);
    if (!isExtractable(request.sourcePath)) {
      fail(MutationError::Unsupported,
           QStringLiteral("“%1” is not an archive File Manager can extract").arg(archive.fileName()));
      return false;
    }
    request.destinationPath = firstFreePath(archive.absolutePath(),
                                            archiveBaseName(archive.fileName()), QString(), planned);
    if (request.destinationPath.isEmpty()) {
      fail(MutationError::AlreadyExists, QStringLiteral("No free name is left for the folder"));
      return false;
    }
    planned.insert(request.destinationPath);
    request.kind = MutationKind::Extract;
    request.declaredRoots = rootsFor(request.sourcePath, request.destinationPath);
    request.expectedSource = identity;
    request.expectedParent = LocalMutationBackend::identityForPath(archive.absolutePath());
    requests.append(std::move(request));
  }
  return submitRequests(MutationKind::Extract, std::move(requests));
}

bool MutationController::isExtractable(const QString &path) const {
  return archiveFormatForName(QFileInfo(path).fileName()) != ArchiveFormat::None;
}

} // namespace QindaQt::Apps::FileManager
