// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_mount_admission.h"
#include "safe_tree_access_p.h"

#include <sys/sysmacros.h>

namespace QindaQt::Apps::FileManager {
namespace {
using namespace SafeTreeAccess;

std::optional<RecoveryMountObservation> observe(int descriptor) {
#if defined(Q_OS_LINUX) && defined(STATX_MNT_ID)
  struct statx status {};
  constexpr unsigned int required = STATX_TYPE | STATX_MODE | STATX_UID |
      STATX_INO | STATX_MNT_ID;
  if (::statx(descriptor, "", AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW,
              required, &status) != 0 ||
      (status.stx_mask & required) != required || status.stx_mnt_id == 0 ||
      !S_ISDIR(status.stx_mode)) return std::nullopt;
  return RecoveryMountObservation{
      status.stx_mnt_id,
      static_cast<quint64>(::makedev(status.stx_dev_major, status.stx_dev_minor)),
      status.stx_ino, status.stx_mode, status.stx_uid};
#else
  Q_UNUSED(descriptor)
  return std::nullopt;
#endif
}
} // namespace

RecoveryDirectoryAdmission::RecoveryDirectoryAdmission(
    int descriptor, QString path, RecoveryMountObservation observation)
    : m_descriptor(descriptor), m_path(std::move(path)),
      m_observation(observation) {}

RecoveryDirectoryAdmission::~RecoveryDirectoryAdmission() {
  if (m_descriptor >= 0) ::close(m_descriptor);
}

RecoveryDirectoryAdmission::RecoveryDirectoryAdmission(
    RecoveryDirectoryAdmission &&other) noexcept
    : m_descriptor(std::exchange(other.m_descriptor, -1)),
      m_path(std::move(other.m_path)), m_observation(other.m_observation) {}

RecoveryDirectoryAdmission &RecoveryDirectoryAdmission::operator=(
    RecoveryDirectoryAdmission &&other) noexcept {
  if (this != &other) {
    if (m_descriptor >= 0) ::close(m_descriptor);
    m_descriptor = std::exchange(other.m_descriptor, -1);
    m_path = std::move(other.m_path);
    m_observation = other.m_observation;
  }
  return *this;
}

std::optional<RecoveryDirectoryAdmission> RecoveryDirectoryAdmission::open(
    const QString &absolutePath, MutationResult &result) {
  if (!absolutePath.startsWith(QLatin1Char('/')) ||
      absolutePath.contains(QChar::Null) ||
      QDir::cleanPath(absolutePath) != absolutePath ||
      QFile::encodeName(absolutePath).size() > 4096) {
    result = failure(MutationError::InvalidRequest, QStringLiteral("Recovery directory path is invalid."));
    return std::nullopt;
  }
  auto descriptor = openAbsoluteDirectory(absolutePath);
  if (!descriptor.valid()) {
    result = failure(errorForErrno(errno), QStringLiteral("Recovery directory could not be admitted."));
    return std::nullopt;
  }
  const auto observed = observe(descriptor.get());
  if (!observed) {
    result = failure(MutationError::Unsupported, QStringLiteral("Live filesystem mount evidence is unavailable."));
    return std::nullopt;
  }
  const int owned = ::fcntl(descriptor.get(), F_DUPFD_CLOEXEC, 0);
  if (owned < 0) {
    result = failure(errorForErrno(errno), QStringLiteral("Recovery directory could not be pinned."));
    return std::nullopt;
  }
  RecoveryDirectoryAdmission admitted(owned, absolutePath, *observed);
  result = admitted.current();
  if (!result.ok()) return std::nullopt;
  return admitted;
}

MutationResult RecoveryDirectoryAdmission::current() const {
  if (m_descriptor < 0)
    return failure(MutationError::Changed, QStringLiteral("Recovery admission has ended."));
  const auto pinned = observe(m_descriptor);
  auto named = openAbsoluteDirectory(m_path);
  const auto currentNamed = named.valid() ? observe(named.get()) : std::nullopt;
  if (!pinned || !currentNamed || *pinned != m_observation ||
      *currentNamed != m_observation)
    return failure(MutationError::Changed, QStringLiteral("Recovery directory or mount changed; preserve all entries."));
  return {};
}

MutationResult RecoveryDirectoryAdmission::privateCurrent() const {
  const auto admitted = current();
  if (!admitted.ok()) return admitted;
  if (m_observation.owner != static_cast<quint32>(::getuid()) ||
      (m_observation.mode & 07777U) != 0700U)
    return failure(MutationError::PermissionDenied, QStringLiteral("Recovery storage requires a private owned directory."));
  return {};
}

} // namespace QindaQt::Apps::FileManager
