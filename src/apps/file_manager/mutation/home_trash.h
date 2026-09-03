// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device_resolver.h"
#include "mutation_types.h"

namespace QindaQt::Apps::FileManager {

// HomeTrash owns only the freedesktop.org Trash v1.0 storage transaction. It
// receives an explicit root (production passes $XDG_DATA_HOME/Trash), never
// discovers or scans mounts, and refuses cross-device moves/restores.
class HomeTrash final {
public:
  HomeTrash(QString root, DeviceResolverPtr deviceResolver);

  [[nodiscard]] MutationResult trash(const MutationRequest &request);
  [[nodiscard]] MutationResult restore(const MutationRequest &request);
  [[nodiscard]] MutationResult empty(const MutationCancellation &cancellation,
                                     const MutationProgressCallback &progress);

private:
  QString m_root;
  DeviceResolverPtr m_deviceResolver;
};

} // namespace QindaQt::Apps::FileManager
