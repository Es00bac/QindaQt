// SPDX-License-Identifier: GPL-3.0-or-later
#include "transfer_types.h"

namespace QindaQt::Apps::FileManager {

QString transferStateName(const TransferState state) {
  switch (state) {
  case TransferState::Queued:
    return QStringLiteral("queued");
  case TransferState::Running:
    return QStringLiteral("running");
  case TransferState::Paused:
    return QStringLiteral("paused");
  case TransferState::Succeeded:
    return QStringLiteral("succeeded");
  case TransferState::Failed:
    return QStringLiteral("failed");
  case TransferState::Cancelled:
    return QStringLiteral("cancelled");
  }
  return QStringLiteral("queued");
}

QString transferOperationName(const TransferOperation operation) {
  return operation == TransferOperation::Move ? QStringLiteral("move")
                                              : QStringLiteral("copy");
}

} // namespace QindaQt::Apps::FileManager
