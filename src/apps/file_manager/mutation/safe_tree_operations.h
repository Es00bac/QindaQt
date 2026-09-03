// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_types.h"

namespace QindaQt::Apps::FileManager {

// Descriptor-relative traversal pins every visited parent and refuses links
// at the kernel boundary. The caller retains policy ownership for declared
// roots and the top-level optimistic identity.
[[nodiscard]] MutationResult copyLocalTreeNoFollow(
    const QString &source, const QString &destination,
    const FileIdentity &expectedDestinationParent,
    const MutationCancellation &cancellation,
    const MutationProgressCallback &progress, int maximumItems);

[[nodiscard]] bool removeLocalTreeNoFollow(const QString &path);

[[nodiscard]] MutationResult emptyLocalDirectoryNoFollow(
    const QString &path, const MutationCancellation &cancellation,
    const MutationProgressCallback &progress, int *removed);

} // namespace QindaQt::Apps::FileManager
