// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "transfer_types.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: runs one already-validated transfer of one source URL into
// one destination folder URL (ADR-0195). TransferQueueController -- not this
// interface -- decides the order, proves both endpoints normalized, refuses a
// folder into its own subtree, and owns every piece of visible state. This
// collaborator never queues, never reorders, never retries, and never
// transfers anything but the one requested item.
//
// An implementation must:
//  - never persist a credential (no wallet/keyring/session write) and never
//    read, log, or accept embedded URL userinfo;
//  - emit finished() at most once per start() that reached the facility, on
//    the GUI thread, never synchronously re-entering the caller inside
//    start();
//  - treat pause/resume/cancel of an unknown or finished id as a no-op, and
//    kill a cancelled transfer quietly. A quiet kill deliberately delivers
//    no finished() at all, so the queue -- not the worker -- retires a
//    cancelled item and fences any late result by the item's own state;
//  - report progress only between start() and finished() for that id.
//
// AGENT-GUARD: a Move is destructive at the source. An implementation must
// never turn a refused or cancelled move into a delete, and must never
// substitute a copy for a move or the reverse.
class TransferWorker : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~TransferWorker() override = default;

  virtual void start(quint64 id, const QUrl &source, const QUrl &destinationFolder,
                     TransferOperation operation) = 0;
  virtual void pause(quint64 id) = 0;
  virtual void resume(quint64 id) = 0;
  virtual void cancel(quint64 id) = 0;

signals:
  // 0..100. Advisory only: an implementation may emit none at all, so the
  // queue must never treat silence as a stall.
  void progressChanged(quint64 id, int percent);
  // Exactly once per accepted start(): an empty diagnostic means success, a
  // bounded human-readable message means a typed failure or cancellation.
  void finished(quint64 id, QString diagnostic);
};

using TransferWorkerPtr = std::unique_ptr<TransferWorker>;

} // namespace QindaQt::Apps::FileManager
