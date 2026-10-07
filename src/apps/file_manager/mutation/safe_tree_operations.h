// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_types.h"

namespace QindaQt::Apps::FileManager {

// Descriptor-relative traversal pins every visited parent and refuses links
// at the kernel boundary. The caller retains policy ownership for declared
// roots and the top-level optimistic identity. Failed copies never delete
// output; typed observations distinguish created partials from replacements.
[[nodiscard]] MutationResult copyLocalTreeNoFollow(
    const QString &source, const QString &destination,
    const FileIdentity &expectedDestinationParent,
    const MutationCancellation &cancellation,
    const MutationProgressCallback &progress, int maximumItems);

// Worker-thread-only, synchronous no-follow readback of a written copy root.
// Carries no mutation authority; missing/replaced ancestry is Unconfirmed.
// Identity/content can change immediately after observation. Never use this
// value to delete, open or restore output without a new owning operation.
[[nodiscard]] MutationOutputObservation observeCopyOutputNoFollow(
    const QString &path, const std::optional<FileIdentity> &writtenIdentity,
    const std::optional<FileIdentity> &parentIdentity, bool copyFinished,
    bool exclusiveCreation);

[[nodiscard]] bool removeLocalTreeNoFollow(const QString &path);

// ADR-0269: Delete Permanently. The item must still be `expected` (lstat
// identity, taken under its pinned parent, so a link is removed as the link
// itself); a folder is removed depth-first without following links.
// Cancellation between entries leaves what was not yet removed in place.
[[nodiscard]] MutationResult deleteLocalTreeNoFollow(
    const QString &path, const FileIdentity &expected,
    const MutationCancellation &cancellation, const MutationProgressCallback &progress);

[[nodiscard]] MutationResult emptyLocalDirectoryNoFollow(
    const QString &path, const MutationCancellation &cancellation,
    const MutationProgressCallback &progress, int *removed);

} // namespace QindaQt::Apps::FileManager
