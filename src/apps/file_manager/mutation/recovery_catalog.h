// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "recovery_record_store.h"
namespace QindaQt::Apps::FileManager {
enum class RecoveryCatalogStep { SyncDirectory, ReadRecord };
// Owning subtractive fixture hook; None still executes actual synchronization
// and readback. Captures live for the synchronous worker-thread call.
using RecoveryCatalogFault = std::function<MutationError(RecoveryCatalogStep)>;
struct RecoveryCatalogRead final {
  MutationResult result;
  QVector<RecoveryRecord> records;
};
// Worker-thread-only owning index. Each immutable prepared record is an
// inspection locator, not restored authority. The catalog lock bounds concurrent
// application windows; records are never evicted, overwritten or replayed.
class RecoveryCatalog final {
public:
  explicit RecoveryCatalog(QString path, RecoveryCatalogFault fault = {});
  ~RecoveryCatalog();
  RecoveryCatalog(const RecoveryCatalog &) = delete;
  RecoveryCatalog &operator=(const RecoveryCatalog &) = delete;
  [[nodiscard]] MutationResult admit(bool create);
  [[nodiscard]] RecoveryCatalogRead inspect() const;
  [[nodiscard]] MutationResult add(const RecoveryRecord &record);
  [[nodiscard]] RecoveryRecordRead lookup(const QString &operationId) const;
private:
  QString m_path;
  RecoveryCatalogFault m_fault;
  std::optional<RecoveryDirectoryAdmission> m_directory;
  bool m_locked = false;
};
// Exclusive private child creation. No existing matching name is reused.
// Returns fresh descriptor/name admission; mkdir/open is not atomic ownership,
// so a failed fence preserves every candidate rather than cleaning by path.
[[nodiscard]] std::optional<RecoveryDirectoryAdmission> createRecoveryDirectory(
    RecoveryDirectoryAdmission &parent, const QString &name, MutationResult &result);
} // namespace QindaQt::Apps::FileManager
