// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "trash_storage.h"

namespace QindaQt::Apps::FileManager {
// Same-mount whole-entry relocation, including a final symlink. The metadata
// reader and both directory admissions stay alive through rename/readback/sync.
// No cleanup, target traversal or replay. Failure may report retained payload
// placement; caller must preserve that value rather than claiming no effects.
[[nodiscard]] MutationResult relocateTrashEntry(
    const QString &source, const RecoveryDirectoryAdmission &sourceParent,
    const QString &destination, const RecoveryDirectoryAdmission &destinationParent,
    const FileIdentity &, const TrashStorage &, const TrashRecord &,
    const MutationCancellation &, const TrashControl &);
}
