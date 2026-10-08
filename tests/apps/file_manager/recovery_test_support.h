// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mutation/cross_volume_move.h"
#include "mutation/local_mutation_backend.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <sys/stat.h>
using namespace QindaQt::Apps::FileManager;
namespace RecoveryTest {
inline bool write(const QString &path, const QByteArray &bytes) {
  QFile fd(path);
  if (!fd.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  return fd.write(bytes) == bytes.size() && fd.flush();
}
inline QByteArray read(const QString &path) {
  QFile fd(path); return fd.open(QIODevice::ReadOnly) ? fd.readAll() : QByteArray{};
}
struct TwoVolumes {
  QTemporaryDir source{QDir::tempPath() + QStringLiteral("/qindaqt-ed05-source-XXXXXX")};
  QTemporaryDir destination{QStringLiteral("/dev/shm/qindaqt-ed05-destination-XXXXXX")};
  QString sourceDirectory = QDir(source.path()).filePath(QStringLiteral("source-parent"));
  QString destinationDirectory = QDir(destination.path()).filePath(QStringLiteral("destination-parent"));
  QString sourcePath = QDir(sourceDirectory).filePath(QStringLiteral("file<literal>"));
  QString destinationPath = QDir(destinationDirectory).filePath(QStringLiteral("moved<literal>"));
  QString catalog = QDir(source.path()).filePath(QStringLiteral("catalog"));
  TwoVolumes() {
    QDir(source.path()).mkdir(QStringLiteral("source-parent"));
    QDir(destination.path()).mkdir(QStringLiteral("destination-parent"));
  }
  [[nodiscard]] bool valid() const {
    const auto a = LocalMutationBackend::identityForPath(sourceDirectory);
    const auto b = LocalMutationBackend::identityForPath(destinationDirectory);
    return source.isValid() && destination.isValid() && a && b && a->device != b->device;
  }
  [[nodiscard]] MutationRequest request() const {
    MutationRequest v; v.kind = MutationKind::Move;
    v.sourcePath = sourcePath; v.destinationPath = destinationPath;
    v.declaredRoots = {source.path(), destination.path()};
    v.expectedSource = LocalMutationBackend::identityForPath(sourcePath);
    v.expectedParent = LocalMutationBackend::identityForPath(destinationDirectory);
    return v;
  }
};
inline QString privatePath(const QString &parent, const QString &prefix) {
  const auto entries = QDir(parent).entryList({prefix + QLatin1Char('*')}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
  return entries.size() == 1 ? QDir(parent).filePath(entries.first()) : QString{};
}
inline QString payload(const QString &directory) { return QDir(directory).filePath(QStringLiteral("payload")); }
}
