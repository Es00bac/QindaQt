// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "archive_codec.h"
#include "device_resolver.h"
#include "home_trash.h"
#include "mutation_backend.h"

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
      ArchiveCodecPtr archives = nullptr);

  [[nodiscard]] MutationResult
  execute(const MutationRequest &request,
          const MutationCancellation &cancellation,
          const MutationProgressCallback &progress) override;

  [[nodiscard]] static std::optional<FileIdentity>
  identityForPath(const QString &path);

private:
  [[nodiscard]] MutationResult createFolder(const MutationRequest &request);
  [[nodiscard]] MutationResult relocate(const MutationRequest &request,
                                        bool renameOnly);
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
  ArchiveCodecPtr m_archives;
};

} // namespace QindaQt::Apps::FileManager
