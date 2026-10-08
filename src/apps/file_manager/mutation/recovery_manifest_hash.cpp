// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_manifest.h"
#include <QCryptographicHash>
#include <QSet>
#include <QtEndian>
#include <limits>
#include <sys/stat.h>

namespace QindaQt::Apps::FileManager {
namespace {
void number(QCryptographicHash &hash, quint64 value) {
  const quint64 encoded = qToBigEndian(value);
  hash.addData(QByteArrayView(reinterpret_cast<const char *>(&encoded), sizeof(encoded)));
}
void bytes(QCryptographicHash &hash, const QByteArray &value) {
  number(hash, static_cast<quint64>(value.size()));
  hash.addData(value);
}
bool valid(const RecoveryManifest &manifest) {
  if (manifest.entries.isEmpty() || manifest.entries.size() > 20'000 ||
      !manifest.entries.front().relativePath.isEmpty()) return false;
  QSet<QByteArray> directories;
  QByteArray previous;
  quint64 total = 0;
  qsizetype budget = 0;
  for (qsizetype index = 0; index < manifest.entries.size(); ++index) {
    const auto &entry = manifest.entries[index];
    const auto &path = entry.relativePath;
    if (path.size() > 4096 || path.contains('\0') ||
        (index > 0 && (path.isEmpty() || path <= previous)) ||
        !entry.identity.valid() || entry.mountId == 0 || entry.identity.size < 0)
      return false;
    if (index > 0) {
      const auto parts = path.split('/');
      if (parts.size() > 128) return false;
      for (const auto &part : parts)
        if (part.isEmpty() || part == "." || part == "..") return false;
      const auto slash = path.lastIndexOf('/');
      const QByteArray parent = slash < 0 ? QByteArray{} : path.left(slash);
      if (!directories.contains(parent)) return false;
    }
    budget += path.size() + 128;
    if (budget > 16 * 1024 * 1024) return false;
    if (S_ISDIR(entry.identity.mode)) {
      if (!entry.contentDigest.isEmpty()) return false;
      directories.insert(path);
    } else if (S_ISREG(entry.identity.mode)) {
      if (entry.contentDigest.size() != 32 ||
          static_cast<quint64>(entry.identity.size) > std::numeric_limits<quint64>::max() - total)
        return false;
      total += static_cast<quint64>(entry.identity.size);
    } else return false;
    previous = path;
  }
  return total == manifest.regularBytes;
}
} // namespace

QByteArray recoveryManifestDigest(const RecoveryManifest &manifest,
                                  bool includeIdentity) {
  if (!valid(manifest)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  bytes(hash, includeIdentity ? QByteArray("recovery-snapshot-1") : QByteArray("recovery-content-1"));
  number(hash, static_cast<quint64>(manifest.entries.size()));
  for (const auto &entry : manifest.entries) {
    bytes(hash, entry.relativePath);
    number(hash, entry.identity.mode);
    number(hash, static_cast<quint64>(entry.identity.modifiedNanoseconds));
    number(hash, S_ISREG(entry.identity.mode) ? static_cast<quint64>(entry.identity.size) : 0);
    bytes(hash, entry.contentDigest);
    if (includeIdentity) {
      number(hash, entry.identity.device);
      number(hash, entry.identity.inode);
      number(hash, static_cast<quint64>(entry.identity.size));
      number(hash, static_cast<quint64>(entry.changedNanoseconds));
      number(hash, entry.mountId);
      number(hash, entry.owner);
      number(hash, entry.group);
    }
  }
  return hash.result();
}

bool sameRecoveryTree(const RecoveryManifest &before,
                       const RecoveryManifest &after, bool allowRootRename) {
  if (!valid(before) || !valid(after) ||
      before.entries.size() != after.entries.size() ||
      before.regularBytes != after.regularBytes) return false;
  for (qsizetype index = 0; index < before.entries.size(); ++index) {
    auto expected = before.entries[index];
    if (index == 0 && allowRootRename)
      expected.changedNanoseconds = after.entries[index].changedNanoseconds;
    if (expected != after.entries[index]) return false;
  }
  return true;
}
} // namespace QindaQt::Apps::FileManager
