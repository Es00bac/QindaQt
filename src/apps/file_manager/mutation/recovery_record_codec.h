// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "recovery_types.h"

namespace QindaQt::Apps::FileManager {

struct RecoveryRecordRead final {
  MutationResult result;
  std::optional<RecoveryRecord> record;
};

// Pure, reentrant owning values. Canonical schema1 JSON only: exact compact
// encoding, decimal strings for lossless 64-bit fields, no duplicate/unknown
// fields, trailing bytes or alternate spellings. Malformed input has no record.
// Encoding invalid values returns empty bytes. Neither function touches storage.
[[nodiscard]] QByteArray encodeRecoveryRecord(const RecoveryRecord &record);
[[nodiscard]] RecoveryRecordRead decodeRecoveryRecord(const QByteArray &bytes);

} // namespace QindaQt::Apps::FileManager
