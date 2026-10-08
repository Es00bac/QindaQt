// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "device_resolver.h"
#include "trash_storage.h"

namespace QindaQt::Apps::FileManager {
// Synchronous worker policy/transaction boundary. Owns immutable configuration
// and resolver, never retains admitted fds between requests or replays receipts.
// Reads below are bounded observations only; mutation re-admits independently.
class VolumeTrash final {
public:
  explicit VolumeTrash(QString homeRoot, DeviceResolverPtr,
      TrashTopDirectoryPtr topology = std::make_shared<TrashTopDirectory>(), TrashControl control = {});
  [[nodiscard]] MutationResult trash(const MutationRequest &, const MutationCancellation &);
  [[nodiscard]] MutationResult restore(const MutationRequest &, const MutationCancellation &);
  [[nodiscard]] static QString originalPathFor(const QString &payload);
  [[nodiscard]] static bool isTrashFilesPath(const QString &path, const QString &homeFiles);
  [[nodiscard]] static QStringList discover(const QString &mountedTopDirectory);
private:
  [[nodiscard]] std::optional<TrashStorage> storageForSource(
      const QString &, MutationResult &) const;
  [[nodiscard]] std::optional<TrashStorage> storageForPayload(
      const QString &, MutationResult &) const;
  QString m_homeRoot;
  DeviceResolverPtr m_devices;
  TrashTopDirectoryPtr m_topology;
  TrashControl m_control;
};
} // namespace QindaQt::Apps::FileManager
