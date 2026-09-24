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

  // ADR-0269 (Put Back): the original path recorded for one item of a
  // Trash's files/ folder, read from the sibling info/<name>.trashinfo
  // (bounded, never through a link) with restore()'s parsing. Empty when the
  // item has no valid record. GUI-thread safe: one small bounded read.
  [[nodiscard]] static QString originalPathFor(const QString &payloadPath);
  // ADR-0269 (Delete Permanently inside Trash): after a payload of this
  // Trash's files/ folder was deleted, removes its record so no orphaned
  // metadata remains. Any other path is ignored.
  void forgetPayload(const QString &payloadPath) const;

private:
  QString m_root;
  DeviceResolverPtr m_deviceResolver;
};

} // namespace QindaQt::Apps::FileManager
