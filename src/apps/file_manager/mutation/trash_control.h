// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mutation_types.h"
namespace QindaQt::Apps::FileManager {
// Constructor-visible synchronous fault/adversarial seam, following the
// existing owning recovery-store pattern. Empty in production. It is neither
// a signal/process supervisor nor permission to perform another user's I/O.
enum class TrashStep {
  OpenRecord, WriteRecord, SyncRecord, SyncInfo, ReadRecord,
  BeforeRename, AfterRename, SyncParents, ReadPayload
};
using TrashControl = std::function<MutationError(TrashStep)>;
[[nodiscard]] inline MutationResult trashControl(const TrashControl &control, TrashStep step) {
  MutationResult result;
  result.error = control ? control(step) : MutationError::None;
  if (!result.ok()) result.diagnostic = QStringLiteral("Trash transaction barrier failed; existing entries and evidence were retained.");
  return result;
}
}
