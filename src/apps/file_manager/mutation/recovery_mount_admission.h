// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "recovery_types.h"

namespace QindaQt::Apps::FileManager {

// Worker-thread-confined, move-only live directory admission. Owns its fd until
// destruction; descriptor() borrows it only for the caller's synchronous call.
// Linux statx mount IDs are observations; retaining this descriptor pins the
// observed mount while current() also reopens the full no-follow pathname.
// Nothing can reconstruct this object from a persisted mount ID. Missing Linux
// evidence refuses with Unsupported; current() is not a lock against later
// rename/mount changes. Every mutation still needs post-syscall readback.
class RecoveryDirectoryAdmission final {
public:
  ~RecoveryDirectoryAdmission();
  RecoveryDirectoryAdmission(RecoveryDirectoryAdmission &&other) noexcept;
  RecoveryDirectoryAdmission &operator=(RecoveryDirectoryAdmission &&other) noexcept;
  RecoveryDirectoryAdmission(const RecoveryDirectoryAdmission &) = delete;
  RecoveryDirectoryAdmission &operator=(const RecoveryDirectoryAdmission &) = delete;

  [[nodiscard]] static std::optional<RecoveryDirectoryAdmission>
  open(const QString &absolutePath, MutationResult &result);
  [[nodiscard]] MutationResult current() const;
  [[nodiscard]] MutationResult privateCurrent() const;
  [[nodiscard]] int descriptor() const { return m_descriptor; }
  [[nodiscard]] const QString &path() const { return m_path; }
  [[nodiscard]] const RecoveryMountObservation &observation() const { return m_observation; }

private:
  RecoveryDirectoryAdmission(int descriptor, QString path,
                             RecoveryMountObservation observation);
  int m_descriptor = -1;
  QString m_path;
  RecoveryMountObservation m_observation;
};

} // namespace QindaQt::Apps::FileManager
