// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_types.h"

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
