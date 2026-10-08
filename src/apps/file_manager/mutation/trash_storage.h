// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "trash_metadata.h"
#include "trash_control.h"
#include "recovery_mount_admission.h"
#include "safe_tree_access_p.h"

namespace QindaQt::Apps::FileManager {
// Synchronous worker-only admission; move-only descriptors die with the call.
// Top directory candidates are freshly admitted, not capabilities. A test
// resolver may constrain top to a disposable subtree; production walks Linux
// mount IDs without consulting another module's service or private storage.
class TrashTopDirectory {
public:
  virtual ~TrashTopDirectory() = default;
  [[nodiscard]] virtual QString forPath(const QString &path, MutationResult &result) const;
};
using TrashTopDirectoryPtr = std::shared_ptr<const TrashTopDirectory>;

// Reader remains held across the deliberate transaction. Its private info
// descriptor borrows the owning TrashStorage lifetime; neither escapes a call.
struct TrashRecord final {
  SafeTreeAccess::UniqueFd reader;
  QByteArray bytes;
  struct stat baseline {};
  QByteArray name;
};
class TrashStorage final {
public:
  TrashStorage(TrashStorage &&) noexcept = default;
  TrashStorage &operator=(TrashStorage &&) noexcept = default;
  TrashStorage(const TrashStorage &) = delete;
  [[nodiscard]] static std::optional<TrashStorage> open(
      const TrashLocation &, bool create, MutationResult &, TrashControl control = {});
  [[nodiscard]] MutationResult current() const;
  [[nodiscard]] MutationResult reserve(const QString &token, const QByteArray &bytes, std::optional<TrashRecord> &record, bool &created) const;
  [[nodiscard]] MutationResult read(const QString &token, std::optional<TrashRecord> &record) const;
  [[nodiscard]] MutationResult recordCurrent(const TrashRecord &) const;
  [[nodiscard]] const TrashLocation &location() const { return m_location; }
  [[nodiscard]] const RecoveryDirectoryAdmission &files() const { return m_files; }
private:
  TrashStorage(TrashLocation, RecoveryDirectoryAdmission,
               RecoveryDirectoryAdmission, RecoveryDirectoryAdmission,
               std::optional<RecoveryDirectoryAdmission>, std::optional<RecoveryDirectoryAdmission>, TrashControl);
  TrashLocation m_location;
  TrashControl m_control;
  RecoveryDirectoryAdmission m_root, m_info, m_files;
  std::optional<RecoveryDirectoryAdmission> m_top, m_shared;
};

[[nodiscard]] bool trashPathWithin(const QString &path, const QString &root);
[[nodiscard]] bool validTrashToken(const QString &token);
[[nodiscard]] QStringList volumeTrashRoots(const QString &topDirectory);
} // namespace QindaQt::Apps::FileManager
