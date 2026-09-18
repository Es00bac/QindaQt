// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "transfer_types.h"
#include "transfer_worker.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: the GUI-thread owner of every network transfer the file
// manager has been asked for (ADR-0195). It owns the order, the one-at-a-time
// dispatch, per-item state, pause/resume/cancel, and the typed refusal
// surface; the injected TransferWorker owns the platform job. It lists no
// folder, shows no dialog, resolves no conflict, and never decides on its own
// to retry.
//
// One source becomes one item, so a selection of ten files is ten items and a
// single failure retires only its own item. Exactly one item runs at a time:
// a saturated link makes concurrent transfers slower, not faster, and one
// running job keeps the platform's credential prompts sequential.
//
// AGENT-GUARD: an item that is not Running or Paused has been retired by the
// user (cancel) or by a completed run. A late worker result for such an item
// is dropped -- this is the fence that makes `cancel` reliable given that a
// quiet KIO kill delivers no result at all.
//
// AGENT-CONTRACT: overwrite/conflict resolution belongs to the platform's
// KIO UI delegate (ADR-0151), not to this class. A QindaQt-owned conflict
// dialog is a later slice; nothing here silently overwrites.
class TransferQueueController final : public QObject {
  Q_OBJECT

  // Every item, oldest first: {id, name, source, destination, operation,
  // state, percent, diagnostic}.
  Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged FINAL)
  Q_PROPERTY(bool busy READ busy NOTIFY itemsChanged FINAL)
  // "Copying x to /path" -- empty when nothing is running or paused.
  Q_PROPERTY(QString activeDescription READ activeDescription NOTIFY itemsChanged FINAL)
  Q_PROPERTY(int activePercent READ activePercent NOTIFY itemsChanged FINAL)
  Q_PROPERTY(bool activePaused READ activePaused NOTIFY itemsChanged FINAL)
  Q_PROPERTY(int queuedCount READ queuedCount NOTIFY itemsChanged FINAL)
  Q_PROPERTY(int failedCount READ failedCount NOTIFY itemsChanged FINAL)
  // The last refusal to accept a request at all (bad address, folder into
  // itself, queue full). Empty until something is refused.
  Q_PROPERTY(QString refusal READ refusal NOTIFY refusalChanged FINAL)

public:
  // Beyond this many live (Queued/Running/Paused) items a request is
  // refused rather than silently trimmed.
  static constexpr int maximumPendingItems = 512;
  // Finished items kept for the banner's history before the oldest is
  // dropped, so a long session cannot grow the list without bound.
  static constexpr int maximumFinishedItems = 64;

  explicit TransferQueueController(TransferWorkerPtr worker,
                                   QObject *parent = nullptr);
  ~TransferQueueController() override;

  // Routes and enqueues one Copy To / Move To request. operationName is
  // "copy" or "move"; sourcePaths are entry paths exactly as the listing
  // published them. Returns true when at least one item was queued; returns
  // false and sets refusal() otherwise. A request TransferRouter assigns to
  // another owner is refused here -- QML asks the router first.
  Q_INVOKABLE bool enqueue(const QStringList &sourcePaths,
                           const QString &destination,
                           const QString &operationName);

  // Names the owner TransferRouter assigns to one Copy To / Move To request:
  // "local", "remote-child", "queue", or "refuse". The destination dialog
  // asks this before dispatching, so exactly one collaborator ever receives
  // a request and the routing rule lives in one tested place rather than in
  // QML conditionals. Pure: it queues nothing and changes no state.
  [[nodiscard]] Q_INVOKABLE QString routeName(const QStringList &sourcePaths,
                                              const QString &destination) const;

  Q_INVOKABLE void pauseActive();
  Q_INVOKABLE void resumeActive();
  // Retires one item by id whatever its state; unknown ids are ignored.
  Q_INVOKABLE void cancel(quint64 id);
  // Retires every live item, in order.
  Q_INVOKABLE void cancelAll();
  // Drops every finished (Succeeded/Failed/Cancelled) item from the list.
  Q_INVOKABLE void clearFinished();
  Q_INVOKABLE void clearRefusal();

  [[nodiscard]] QVariantList items() const;
  [[nodiscard]] bool busy() const;
  [[nodiscard]] QString activeDescription() const;
  [[nodiscard]] int activePercent() const;
  [[nodiscard]] bool activePaused() const;
  [[nodiscard]] int queuedCount() const;
  [[nodiscard]] int failedCount() const;
  [[nodiscard]] QString refusal() const { return m_refusal; }

  // Test seam independent of QML's QVariantList marshalling.
  [[nodiscard]] QVector<TransferItem> itemValues() const { return m_items; }

signals:
  void itemsChanged();
  void refusalChanged();
  // One item finished successfully into this folder. The window re-reads the
  // authoritative listing when it is the folder being browsed; the queue
  // itself never refreshes or navigates.
  void transferCommitted(const QUrl &destinationFolder);

private:
  void setRefusal(const QString &message);
  // Marks one live item Cancelled and tells the worker, without dispatching
  // anything. Returns false for an unknown or already-retired id.
  bool retire(quint64 id);
  void startNext();
  void onWorkerProgress(quint64 id, int percent);
  void onWorkerFinished(quint64 id, const QString &diagnostic);
  [[nodiscard]] int indexOf(quint64 id) const;
  void trimFinished();

  TransferWorkerPtr m_worker;
  QVector<TransferItem> m_items;
  quint64 m_nextId = 1;
  // 0 when nothing is dispatched. Only this id may be advanced by a worker
  // signal.
  quint64 m_activeId = 0;
  QString m_refusal;
};

} // namespace QindaQt::Apps::FileManager
