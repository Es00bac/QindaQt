// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "recovery_mount_admission.h"
#include "recovery_record_codec.h"

namespace QindaQt::Apps::FileManager {

enum class RecoveryStoreStep { CreateRecord, WriteRecord, SyncRecord, SyncDirectory, ReadRecord };
// Test-only failure injection is subtractive: an error refuses the next real
// syscall/readback. Returning None still executes it; this port cannot assert durability.
using RecoveryStoreFault = std::function<MutationError(RecoveryStoreStep)>;

struct RecoveryRecordWrite final {
  MutationResult result;
  bool entryCreated = false;
  bool fileSynced = false;
  bool directorySynced = false;
};

// Borrows a private live directory for its whole lifetime, same worker thread.
// Append-only sequence slots, exclusive no-follow files, 0600, fsync file+dir.
// No overwrite, unlink, cleanup, action replay or grant reconstruction exists.
// A partial slot is retained and makes later append refuse; inspect() may return
// the preceding valid record alongside an error, always inspection evidence.
// Fresh restore admission is outside this store. The fault callback is optional
// owning test state and must not outlive its captures.
class RecoveryRecordStore final {
public:
  explicit RecoveryRecordStore(RecoveryDirectoryAdmission &directory,
                               RecoveryStoreFault fault = {});
  [[nodiscard]] RecoveryRecordRead inspect() const;
  [[nodiscard]] RecoveryRecordWrite append(const RecoveryRecord &record);
private:
  [[nodiscard]] MutationResult fault(RecoveryStoreStep step) const;
  RecoveryDirectoryAdmission &m_directory;
  RecoveryStoreFault m_fault;
};

} // namespace QindaQt::Apps::FileManager
