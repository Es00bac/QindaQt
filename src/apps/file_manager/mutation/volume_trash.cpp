// SPDX-License-Identifier: GPL-3.0-or-later
#include "volume_trash.h"
#include "trash_relocate.h"
#include "safe_tree_access_p.h"
#include <QDateTime>
#include <algorithm>

namespace QindaQt::Apps::FileManager {
namespace {
using namespace SafeTreeAccess;
bool validPath(const QString &path) {
  return path.startsWith(QLatin1Char('/')) && !path.contains(QChar::Null) &&
      QDir::cleanPath(path) == path && QFile::decodeName(QFile::encodeName(path)) == path;
}
bool declared(const QString &path, const QStringList &roots) {
  return std::any_of(roots.cbegin(), roots.cend(), [&](const auto &root) {
    return validPath(root) && trashPathWithin(path, root);
  });
}
bool trashCancelled(const MutationCancellation &cancel) {
  return cancel && cancel->load(std::memory_order_relaxed);
}
TrashLocation locationForPayload(const QString &payload) {
  const auto files = QFileInfo(payload).absolutePath();
  if (QFileInfo(files).fileName() != QStringLiteral("files")) return {};
  const auto root = QFileInfo(files).absolutePath();
  const auto uid = QString::number(::getuid());
  if (QFileInfo(root).fileName() == QStringLiteral(".Trash-") + uid)
    return {root, QFileInfo(root).absolutePath(), false};
  const auto shared = QFileInfo(root).absolutePath();
  if (QFileInfo(root).fileName() == uid && QFileInfo(shared).fileName() == QStringLiteral(".Trash"))
    return {root, QFileInfo(shared).absolutePath(), false};
  return {root, {}, true};
}
QString tokenBase(const QString &source) {
  QString result = QFileInfo(source).fileName();
  while (QFile::encodeName(result).size() > 180) result.chop(1);
  // Chopping UTF16 can split a surrogate. Remove it instead of substituting.
  while (!result.isEmpty() && QFile::decodeName(QFile::encodeName(result)) != result)
    result.chop(1);
  return result.isEmpty() ? QStringLiteral("item") : result;
}
void retainedMetadata(MutationResult &result, const TrashLocation &location,
                      const QString &token, const QString &original) {
  result.trashReceipt = {location.root, location.topDirectory,
      QDir(location.root).filePath(QStringLiteral("files/") + token),
      QDir(location.root).filePath(QStringLiteral("info/") + token + QStringLiteral(".trashinfo")),
      original, false, true};
}
} // namespace
VolumeTrash::VolumeTrash(QString homeRoot, DeviceResolverPtr devices, TrashTopDirectoryPtr topology, TrashControl control)
    : m_homeRoot(QDir::cleanPath(std::move(homeRoot))), m_devices(std::move(devices)),
      m_topology(std::move(topology)), m_control(std::move(control)) {}
std::optional<TrashStorage> VolumeTrash::storageForSource(
    const QString &source, MutationResult &result) const {
  const auto sourceDevice = m_devices->deviceForPath(source);
  const auto homeDevice = m_devices->deviceForPath(m_homeRoot);
  if (!sourceDevice || !homeDevice) {
    result = failure(MutationError::Vanished, QStringLiteral("Trash filesystem observation is unavailable."));
    return std::nullopt;
  }
  if (*sourceDevice == *homeDevice)
    return TrashStorage::open({m_homeRoot, {}, true}, true, result, m_control);
  const auto top = m_topology->forPath(source, result);
  if (top.isEmpty()) return std::nullopt;
  auto admitted = RecoveryDirectoryAdmission::open(top, result);
  if (!admitted) return std::nullopt;
  if (!trashPathWithin(source, top) || admitted->observation().device != *sourceDevice) {
    result = failure(MutationError::CrossDevice, QStringLiteral("Observed Trash filesystem does not match source; it was retained."));
    return std::nullopt;
  }
  // A custom admission seam may deny a device even when the real mount is
  // the home mount; never convert injected refusal into a cross-volume claim.
  auto homeParent = RecoveryDirectoryAdmission::open(QFileInfo(m_homeRoot).absolutePath(), result);
  if (homeParent && homeParent->observation().device == admitted->observation().device) {
    result = failure(MutationError::CrossDevice, QStringLiteral("A distinct Trash filesystem could not be confirmed."));
    return std::nullopt;
  }
  for (const auto &root : volumeTrashRoots(top)) {
    auto storage = TrashStorage::open({root, top, false}, true, result, m_control);
    if (storage) return storage;
  }
  return std::nullopt;
}
std::optional<TrashStorage> VolumeTrash::storageForPayload(
    const QString &payload, MutationResult &result) const {
  const auto location = locationForPayload(payload);
  if (location.root == m_homeRoot && location.home)
    return TrashStorage::open(location, false, result, m_control);
  if (location.home) {
    result = failure(MutationError::InvalidRequest, QStringLiteral("Payload does not belong to this Trash store."));
    return std::nullopt;
  }
  const auto top = m_topology->forPath(payload, result);
  if (top.isEmpty()) return std::nullopt;
  if (top != location.topDirectory) {
    result = failure(MutationError::Changed, QStringLiteral("Payload is not in the current volume Trash."));
    return std::nullopt;
  }
  return TrashStorage::open(location, false, result, m_control);
}
MutationResult VolumeTrash::trash(const MutationRequest &request, const MutationCancellation &cancel) {
  if (!validPath(request.sourcePath) || !declared(request.sourcePath, request.declaredRoots) ||
      !request.expectedSource || !request.expectedSource->valid())
    return failure(MutationError::InvalidRequest, QStringLiteral("Trash requires a representable path and listing identity."));
  if (trashPathWithin(request.sourcePath, m_homeRoot))
    return failure(MutationError::InvalidRequest, QStringLiteral("An item already in Trash cannot be trashed again."));
  if (trashCancelled(cancel)) return failure(MutationError::Cancelled, QStringLiteral("Trash cancelled before admission."));
  MutationResult result;
  auto parent = RecoveryDirectoryAdmission::open(QFileInfo(request.sourcePath).absolutePath(), result);
  if (!parent) return result;
  struct stat source {};
  const auto name = QFile::encodeName(QFileInfo(request.sourcePath).fileName());
  if (::fstatat(parent->descriptor(), name.constData(), &source, AT_SYMLINK_NOFOLLOW) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Trash source is unavailable."));
  if (identity(source) != *request.expectedSource)
    return failure(MutationError::Changed, QStringLiteral("Trash source changed since listing."));
  if (!S_ISREG(source.st_mode) && !S_ISDIR(source.st_mode) && !S_ISLNK(source.st_mode))
    return failure(MutationError::Unsupported, QStringLiteral("Only files, directories and symbolic-link entries can be trashed."));
  auto storage = storageForSource(request.sourcePath, result);
  if (!storage) return result;
  if (trashPathWithin(request.sourcePath, storage->location().root))
    return failure(MutationError::InvalidRequest, QStringLiteral("An item already in Trash cannot be trashed again."));
  if (!storage->location().home)
    for (const auto &root : volumeTrashRoots(storage->location().topDirectory))
      if (trashPathWithin(request.sourcePath, root))
        return failure(MutationError::InvalidRequest, QStringLiteral("Volume Trash entries cannot be trashed again."));
  QByteArray bytes;
  result = TrashMetadataCodec::encode(request.sourcePath, storage->location(),
      QDateTime::currentDateTime(), bytes);
  if (!result.ok()) return result;
  const auto base = tokenBase(request.sourcePath);
  for (int suffix = 0; suffix < 10000; ++suffix) {
    if (trashCancelled(cancel)) return failure(MutationError::Cancelled, QStringLiteral("Trash cancelled; existing entries retained."));
    const auto token = suffix == 0 ? base : base + QLatin1Char('.') + QString::number(suffix);
    const auto encoded = QFile::encodeName(token);
    struct stat existing {};
    if (::fstatat(storage->files().descriptor(), encoded.constData(), &existing, AT_SYMLINK_NOFOLLOW) == 0) continue;
    if (errno != ENOENT) return failure(errorForErrno(errno), QStringLiteral("Trash payload collision cannot be checked."));
    std::optional<TrashRecord> record;
    bool created = false;
    result = storage->reserve(token, bytes, record, created);
    if (result.error == MutationError::AlreadyExists) continue;
    if (!result.ok()) {
      if (created) retainedMetadata(result, storage->location(), token, request.sourcePath);
      return result;
    }
    const auto payload = QDir(storage->location().root).filePath(QStringLiteral("files/") + token);
    result = relocateTrashEntry(request.sourcePath, *parent, payload,
        storage->files(), *request.expectedSource, *storage, *record, cancel, m_control);
    retainedMetadata(result, storage->location(), token, request.sourcePath);
    if (!result.ok()) return result;
    result.trashReceipt.payloadConfirmed = true;
    result.trashToken = token;
    result.originalPath = request.sourcePath;
    result.diagnostic = QStringLiteral("Moved to recoverable Trash.");
    return result;
  }
  return failure(MutationError::AlreadyExists, QStringLiteral("Trash name allocation reached its bounded limit; source retained."));
}
MutationResult VolumeTrash::restore(const MutationRequest &request, const MutationCancellation &cancel) {
  if (!validTrashToken(request.trashToken) || !request.expectedSource ||
      !request.expectedParent || !validPath(request.destinationPath) ||
      !declared(request.destinationPath, request.declaredRoots))
    return failure(MutationError::InvalidRequest, QStringLiteral("Restore requires payload and destination-parent observations."));
  const auto payload = request.sourcePath.isEmpty()
      ? QDir(m_homeRoot).filePath(QStringLiteral("files/") + request.trashToken) : request.sourcePath;
  if (!validPath(payload) || QFileInfo(payload).fileName() != request.trashToken)
    return failure(MutationError::InvalidRequest, QStringLiteral("Restore payload and token disagree."));
  if (trashCancelled(cancel)) return failure(MutationError::Cancelled, QStringLiteral("Restore cancelled; payload retained."));
  MutationResult result;
  auto storage = storageForPayload(payload, result);
  if (!storage) return result;
  std::optional<TrashRecord> record;
  result = storage->read(request.trashToken, record);
  if (!result.ok()) return result;
  TrashMetadata metadata;
  result = TrashMetadataCodec::decode(record->bytes, storage->location(), metadata);
  if (!result.ok()) return result;
  if ((!request.restoreToChosenFolder && request.destinationPath != metadata.originalPath) ||
      (request.restoreToChosenFolder &&
       QFileInfo(request.destinationPath).fileName() != QFileInfo(metadata.originalPath).fileName()))
    return failure(MutationError::InvalidRequest, QStringLiteral("Restore destination does not match the original metadata name."));
  if (trashPathWithin(request.destinationPath, storage->location().root))
    return failure(MutationError::InvalidRequest, QStringLiteral("Choose a restore folder outside this Trash store."));
  const auto device = m_devices->deviceForPath(QFileInfo(request.destinationPath).absolutePath());
  const auto sourceDevice = m_devices->deviceForPath(payload);
  if (!device || !sourceDevice) return failure(MutationError::Vanished, QStringLiteral("Restore filesystem unavailable."));
  if (*device != *sourceDevice) return failure(MutationError::CrossDevice, QStringLiteral("Chosen restore folder is on another filesystem; payload retained."));
  auto parent = RecoveryDirectoryAdmission::open(QFileInfo(request.destinationPath).absolutePath(), result);
  if (!parent) return result;
  struct stat status {};
  if (::fstat(parent->descriptor(), &status) != 0 || identity(status) != *request.expectedParent)
    return failure(MutationError::Changed, QStringLiteral("Restore parent changed since selection."));
  result = relocateTrashEntry(payload, storage->files(), request.destinationPath,
      *parent, *request.expectedSource, *storage, *record, cancel, m_control);
  retainedMetadata(result, storage->location(), request.trashToken, metadata.originalPath);
  if (result.ok()) {
    result.trashReceipt.payloadConfirmed = false;
    result.trashReceipt.restoredConfirmed = true;
    result.diagnostic = QStringLiteral("Restored the payload; its metadata-only record was retained.");
  }
  return result;
}
QString VolumeTrash::originalPathFor(const QString &payload) {
  if (!validPath(payload)) return {};
  MutationResult result;
  const auto location = locationForPayload(payload);
  auto storage = TrashStorage::open(location, false, result);
  if (!storage) return {};
  std::optional<TrashRecord> record;
  if (!storage->read(QFileInfo(payload).fileName(), record).ok()) return {};
  TrashMetadata metadata;
  if (!TrashMetadataCodec::decode(record->bytes, location, metadata).ok()) return {};
  return metadata.originalPath;
}
bool VolumeTrash::isTrashFilesPath(const QString &path, const QString &homeFiles) {
  if (path == homeFiles) return true;
  const auto location = locationForPayload(QDir(path).filePath(QStringLiteral("observation")));
  if (location.home) return false;
  // Menu classification only: passive QML must not probe a filesystem.
  // Deliberate metadata read and the worker restore independently admit it.
  return validPath(path) && volumeTrashRoots(location.topDirectory).contains(location.root);
}
QStringList VolumeTrash::discover(const QString &top) {
  QStringList result;
  for (const auto &root : volumeTrashRoots(top)) {
    MutationResult admission;
    if (TrashStorage::open({root, top, false}, false, admission))
      result.append(QDir(root).filePath(QStringLiteral("files")));
  }
  return result;
}
} // namespace QindaQt::Apps::FileManager
