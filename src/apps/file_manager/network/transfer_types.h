// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUrl>

namespace QindaQt::Apps::FileManager {

enum class TransferOperation {
  Copy,
  // A move is destructive at the source. Every guard that refuses a copy
  // must refuse the same move, and the queue never retries a move on its own.
  Move,
};

// Which side of the local/network boundary an endpoint lives on. The queue
// exists precisely because a transfer may cross it; a transfer with no
// remote endpoint belongs to MutationController instead, which has the
// identity checks, Trash, and undo a local operation is expected to have.
enum class TransferRealm {
  Local,
  Remote,
};

enum class TransferState {
  Queued,
  Running,
  Paused,
  Succeeded,
  Failed,
  Cancelled,
};

// One queued copy or move of one source into one destination folder. A
// multi-item selection becomes one TransferItem per source so a single
// failure retires only its own item and the rest of the selection still runs.
struct TransferItem final {
  quint64 id = 0;
  QUrl source;
  QUrl destinationFolder;
  TransferOperation operation = TransferOperation::Copy;
  TransferState state = TransferState::Queued;
  // 0..100 while Running; the last reported value once finished.
  int percent = 0;
  // The source's own file name, for the banner and the accessible name.
  QString name;
  // Bounded human-readable text for Failed; empty otherwise.
  QString diagnostic;
};

[[nodiscard]] QString transferStateName(TransferState state);
[[nodiscard]] QString transferOperationName(TransferOperation operation);

} // namespace QindaQt::Apps::FileManager
