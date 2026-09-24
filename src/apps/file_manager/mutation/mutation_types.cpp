// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_types.h"

#include <cerrno>

namespace QindaQt::Apps::FileManager {

QString mutationErrorKey(MutationError error) {
  switch (error) {
  case MutationError::None:
    return QStringLiteral("none");
  case MutationError::InvalidRequest:
    return QStringLiteral("invalid-request");
  case MutationError::PermissionDenied:
    return QStringLiteral("permission-denied");
  case MutationError::AlreadyExists:
    return QStringLiteral("already-exists");
  case MutationError::CrossDevice:
    return QStringLiteral("cross-device");
  case MutationError::DiskFull:
    return QStringLiteral("disk-full");
  case MutationError::Vanished:
    return QStringLiteral("vanished");
  case MutationError::Changed:
    return QStringLiteral("changed");
  case MutationError::SymlinkEscape:
    return QStringLiteral("symlink-escape");
  case MutationError::Cancelled:
    return QStringLiteral("cancelled");
  case MutationError::Unsupported:
    return QStringLiteral("unsupported");
  case MutationError::IoError:
    return QStringLiteral("io-error");
  case MutationError::Busy:
    return QStringLiteral("busy");
  }
  return QStringLiteral("io-error");
}

MutationError mutationErrorForErrno(int error) {
  switch (error) {
  case EACCES:
  case EPERM:
    return MutationError::PermissionDenied;
  case EEXIST:
  case ENOTEMPTY:
    return MutationError::AlreadyExists;
  case EXDEV:
    return MutationError::CrossDevice;
  case ENOSPC:
#ifdef EDQUOT
  case EDQUOT:
#endif
    return MutationError::DiskFull;
  case ENOENT:
  case ENOTDIR:
    return MutationError::Vanished;
  case ENAMETOOLONG:
  case EINVAL:
    return MutationError::InvalidRequest;
  case ELOOP:
    return MutationError::SymlinkEscape;
  default:
    return MutationError::IoError;
  }
}

QString boundedMutationDiagnostic(const QString &message) {
  QString sanitized;
  sanitized.reserve(qMin(message.size(), qsizetype{512}));
  for (const QChar character : message) {
    if (sanitized.size() >= 512) {
      break;
    }
    sanitized.append(character.category() == QChar::Other_Control
                         ? QChar::ReplacementCharacter
                         : character);
  }
  return sanitized.trimmed().isEmpty() ? QStringLiteral("The operation failed")
                                       : sanitized.trimmed();
}

} // namespace QindaQt::Apps::FileManager
