// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_controller.h"
#include "local_mutation_backend.h"
#include "volume_trash.h"
#include <QDir>
#include <QFileInfo>

namespace QindaQt::Apps::FileManager {
QString MutationController::originalTrashPath(const QString &payload) const {
  return VolumeTrash::originalPathFor(payload);
}
bool MutationController::isTrashFilesPath(const QString &path, const QString &homeFiles) const {
  return VolumeTrash::isTrashFilesPath(path, homeFiles);
}
bool MutationController::putBackItems(const QVariantList &items) {
  return putBackItemsTo(items, {});
}
bool MutationController::putBackItemsTo(const QVariantList &items, const QString &folder) {
  if (!folder.isEmpty() && (!folder.startsWith(QLatin1Char('/')) ||
      QDir::cleanPath(folder) != folder || folder.contains(QChar::Null))) {
    fail(MutationError::InvalidRequest, QStringLiteral("Choose an existing absolute local restore folder."));
    return false;
  }
  QVector<MutationRequest> requests;
  for (const auto &item : items) {
    QString payload;
    FileIdentity identity;
    if (!parseItem(item, &payload, &identity)) return false;
    const auto original = VolumeTrash::originalPathFor(payload);
    if (original.isEmpty()) {
      fail(MutationError::InvalidRequest, QStringLiteral("This payload has no safe Trash record; it was retained."));
      return false;
    }
    const auto parent = folder.isEmpty() ? QFileInfo(original).absolutePath() : folder;
    MutationRequest request;
    request.expectedParent = LocalMutationBackend::identityForPath(parent);
    if (!request.expectedParent) {
      fail(MutationError::Vanished, QStringLiteral("The original folder is unavailable. Choose an existing restore folder; payloads remain in Trash."));
      return false;
    }
    request.kind = MutationKind::Restore;
    request.sourcePath = payload;
    request.trashToken = QFileInfo(payload).fileName();
    request.destinationPath = folder.isEmpty() ? original
        : QDir(folder).filePath(QFileInfo(original).fileName());
    request.declaredRoots = {parent};
    request.expectedSource = identity;
    request.restoreToChosenFolder = !folder.isEmpty();
    requests.append(std::move(request));
  }
  return submitRequests(MutationKind::Restore, std::move(requests));
}
bool MutationController::restoreLast() { return restoreLastTo({}); }
bool MutationController::restoreLastTo(const QString &folder) {
  if (!canRestore()) {
    fail(MutationError::InvalidRequest, QStringLiteral("There is no recoverable Trash item."));
    return false;
  }
  if (!folder.isEmpty() && (!folder.startsWith(QLatin1Char('/')) ||
      QDir::cleanPath(folder) != folder || folder.contains(QChar::Null))) {
    fail(MutationError::InvalidRequest, QStringLiteral("Choose an existing absolute local restore folder."));
    return false;
  }
  const auto parent = folder.isEmpty() ? QFileInfo(m_lastTrashOriginalPath).absolutePath() : folder;
  MutationRequest request;
  request.expectedParent = LocalMutationBackend::identityForPath(parent);
  if (!request.expectedParent) {
    fail(MutationError::Vanished, QStringLiteral("The original folder is unavailable. Choose an existing restore folder; the payload remains in Trash."));
    return false;
  }
  request.kind = MutationKind::Restore;
  request.sourcePath = m_lastTrashPayloadPath;
  request.trashToken = m_lastTrashToken;
  request.destinationPath = folder.isEmpty() ? m_lastTrashOriginalPath
      : QDir(folder).filePath(QFileInfo(m_lastTrashOriginalPath).fileName());
  request.declaredRoots = {parent};
  request.expectedSource = m_lastTrashIdentity;
  request.restoreToChosenFolder = !folder.isEmpty();
  return submit(std::move(request));
}
} // namespace QindaQt::Apps::FileManager
