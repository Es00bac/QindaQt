// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "archive_codec.h"
#include "device_resolver.h"
#include "home_trash.h"
#include "volume_trash.h"
#include "mutation_backend.h"
#include "cross_volume_move.h"

namespace QindaQt::Apps::FileManager {

class LocalMutationBackend final : public MutationBackend {
public:
  static constexpr int maximumCopiedItems = 20'000;

  // `archives` (ADR-0269) serves Compress and Extract; without one both
  // report Unsupported. File Manager passes its KArchive codec; the Desktop
  // boundary's controller passes none yet.
  explicit LocalMutationBackend(
      QString homeTrashRoot,
      DeviceResolverPtr deviceResolver = std::make_shared<LocalDeviceResolver>(),
      ArchiveCodecPtr archives = nullptr, QString recoveryCatalogPath = {},
      TrashTopDirectoryPtr trashTopology = std::make_shared<TrashTopDirectory>());

  [[nodiscard]] MutationResult
  execute(const MutationRequest &request,
          const MutationCancellation &cancellation,
          const MutationProgressCallback &progress) override;

  [[nodiscard]] static std::optional<FileIdentity>
  identityForPath(const QString &path);

private:
  [[nodiscard]] MutationResult createFolder(const MutationRequest &request);
  [[nodiscard]] MutationResult relocate(const MutationRequest &request,
                                        bool renameOnly, const MutationCancellation &cancellation,
                                        const MutationProgressCallback &progress);
  [[nodiscard]] MutationResult copy(const MutationRequest &request,
                                    const MutationCancellation &cancellation,
                                    const MutationProgressCallback &progress);
  // ADR-0269 kinds; each validates roots, paths and identities first.
  [[nodiscard]] MutationResult createFile(const MutationRequest &request);
  [[nodiscard]] MutationResult link(const MutationRequest &request);
  [[nodiscard]] MutationResult remove(const MutationRequest &request,
                                      const MutationCancellation &cancellation,
                                      const MutationProgressCallback &progress);
  [[nodiscard]] MutationResult compress(const MutationRequest &request,
                                        const MutationCancellation &cancellation,
                                        const MutationProgressCallback &progress);
  [[nodiscard]] MutationResult extract(const MutationRequest &request,
                                       const MutationCancellation &cancellation,
                                       const MutationProgressCallback &progress);
  DeviceResolverPtr m_deviceResolver;
  HomeTrash m_homeTrash;
  VolumeTrash m_volumeTrash;
  ArchiveCodecPtr m_archives;
  CrossVolumeMove m_crossVolume;
};

} // namespace QindaQt::Apps::FileManager
