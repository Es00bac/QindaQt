// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mutation/local_mutation_backend.h"
#include "mutation/volume_trash.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QVariantMap>
#include <sys/stat.h>
#include <unistd.h>

namespace TrashTest {
using namespace QindaQt::Apps::FileManager;
class FixedTop final : public TrashTopDirectory {
public:
  explicit FixedTop(QString root) : m_root(std::move(root)) {}
  QString forPath(const QString &, MutationResult &result) const override {
    result = {}; return m_root;
  }
private:
  QString m_root;
};
inline bool write(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      file.write(bytes) != bytes.size()) return false;
  file.close();
  return ::chmod(QFile::encodeName(path).constData(), 0600) == 0;
}
inline QByteArray read(const QString &path) {
  QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
inline bool privateDir(const QString &path) {
  return QDir().mkpath(path) && ::chmod(QFile::encodeName(path).constData(), 0700) == 0;
}
inline QVariantMap identityMap(const FileIdentity &value, const QString &path) {
  return {{"path", path}, {"device", QString::number(value.device)},
      {"inode", QString::number(value.inode)}, {"identitySize", QString::number(value.size)},
      {"modifiedNanoseconds", QString::number(value.modifiedNanoseconds)},
      {"mode", QString::number(value.mode)}};
}
struct Fixture final {
  QTemporaryDir home;
  QTemporaryDir volume{QStringLiteral("/dev/shm/qindaqt-ed06-trash-XXXXXX")};
  std::shared_ptr<FixedTop> top = std::make_shared<FixedTop>(volume.path());
  MutationCancellation cancel = std::make_shared<std::atomic_bool>(false);
  bool admit() const {
    const auto a = LocalMutationBackend::identityForPath(home.path());
    const auto b = LocalMutationBackend::identityForPath(volume.path());
    return home.isValid() && volume.isValid() && a && b && a->device != b->device &&
        privateDir(sourceFolder());
  }
  QString homeRoot() const { return home.filePath(QStringLiteral("Trash")); }
  QString sourceFolder() const { return volume.filePath(QStringLiteral("source")); }
  QString source(const QString &name = QStringLiteral("item")) const { return QDir(sourceFolder()).filePath(name); }
  QString privateRoot() const { return volume.filePath(QStringLiteral(".Trash-") + QString::number(::getuid())); }
  VolumeTrash owner(TrashControl control = {}) const {
    return VolumeTrash(homeRoot(), std::make_shared<LocalDeviceResolver>(), top, std::move(control));
  }
  MutationRequest trash(const QString &path) const {
    MutationRequest request;
    request.kind = MutationKind::Trash; request.sourcePath = path;
    request.expectedSource = LocalMutationBackend::identityForPath(path);
    request.declaredRoots = {QFileInfo(path).absolutePath()}; return request;
  }
  MutationRequest restore(const MutationResult &saved, const QString &destination = {}) const {
    MutationRequest request;
    request.kind = MutationKind::Restore; request.sourcePath = saved.outputPath;
    request.trashToken = saved.trashToken;
    request.destinationPath = destination.isEmpty() ? saved.originalPath : destination;
    request.restoreToChosenFolder = !destination.isEmpty();
    request.expectedSource = saved.outputIdentity;
    const auto parent = QFileInfo(request.destinationPath).absolutePath();
    request.expectedParent = LocalMutationBackend::identityForPath(parent);
    request.declaredRoots = {parent}; return request;
  }
};
} // namespace TrashTest
