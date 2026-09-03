// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device_resolver.h"
#include "home_trash.h"
#include "mutation_backend.h"

namespace QindaQt::Apps::FileManager {

class LocalMutationBackend final : public MutationBackend {
public:
  static constexpr int maximumCopiedItems = 20'000;

  explicit LocalMutationBackend(
      QString homeTrashRoot,
      DeviceResolverPtr deviceResolver = std::make_shared<LocalDeviceResolver>());

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
  DeviceResolverPtr m_deviceResolver;
  HomeTrash m_homeTrash;
};

} // namespace QindaQt::Apps::FileManager
