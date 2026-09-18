// SPDX-License-Identifier: GPL-3.0-or-later
#include "transfer_queue_controller.h"

#include "network_location.h"
#include "transfer_router.h"

#include <QVariantMap>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] bool isLive(const TransferState state) {
  return state == TransferState::Queued || state == TransferState::Running ||
         state == TransferState::Paused;
}

// The last path segment of either realm, which is what the user recognizes.
[[nodiscard]] QString displayNameFor(const QUrl &source) {
  const QString name = source.fileName();
  return name.isEmpty() ? source.toString() : name;
}

// A destination folder is shown as a local path or as its full address; a
// transfer never shows userinfo because no endpoint may carry any.
[[nodiscard]] QString destinationText(const QUrl &destinationFolder) {
  return destinationFolder.isLocalFile() ? destinationFolder.toLocalFile()
                                         : destinationFolder.toString();
}

[[nodiscard]] QVariantMap asVariant(const TransferItem &item) {
  return {{QStringLiteral("id"), QVariant::fromValue(item.id)},
          {QStringLiteral("name"), item.name},
          {QStringLiteral("source"), item.source.toString()},
          {QStringLiteral("destination"), destinationText(item.destinationFolder)},
          {QStringLiteral("operation"), transferOperationName(item.operation)},
          {QStringLiteral("state"), transferStateName(item.state)},
          {QStringLiteral("percent"), item.percent},
          {QStringLiteral("diagnostic"), item.diagnostic}};
}

} // namespace

TransferQueueController::TransferQueueController(TransferWorkerPtr worker,
                                                 QObject *parent)
    : QObject(parent), m_worker(std::move(worker)) {
  if (m_worker) {
    connect(m_worker.get(), &TransferWorker::progressChanged, this,
            &TransferQueueController::onWorkerProgress);
    connect(m_worker.get(), &TransferWorker::finished, this,
            &TransferQueueController::onWorkerFinished);
  }
}

// The worker owns its platform jobs and kills them on destruction; the queue
// only has to stop looking at them.
TransferQueueController::~TransferQueueController() = default;

int TransferQueueController::indexOf(const quint64 id) const {
  for (int index = 0; index < m_items.size(); ++index) {
    if (m_items.at(index).id == id) {
      return index;
    }
  }
  return -1;
}

QVariantList TransferQueueController::items() const {
  QVariantList published;
  published.reserve(m_items.size());
  for (const TransferItem &item : m_items) {
    published.append(asVariant(item));
  }
  return published;
}

bool TransferQueueController::busy() const {
  for (const TransferItem &item : m_items) {
    if (isLive(item.state)) {
      return true;
    }
  }
  return false;
}

QString TransferQueueController::activeDescription() const {
  const int index = indexOf(m_activeId);
  if (index < 0) {
    return {};
  }
  const TransferItem &item = m_items.at(index);
  return item.operation == TransferOperation::Move
             ? QStringLiteral("Moving %1 to %2")
                   .arg(item.name, destinationText(item.destinationFolder))
             : QStringLiteral("Copying %1 to %2")
                   .arg(item.name, destinationText(item.destinationFolder));
}

int TransferQueueController::activePercent() const {
  const int index = indexOf(m_activeId);
  return index < 0 ? 0 : m_items.at(index).percent;
}

bool TransferQueueController::activePaused() const {
  const int index = indexOf(m_activeId);
  return index >= 0 && m_items.at(index).state == TransferState::Paused;
}

int TransferQueueController::queuedCount() const {
  int count = 0;
  for (const TransferItem &item : m_items) {
    if (item.state == TransferState::Queued) {
      ++count;
    }
  }
  return count;
}

int TransferQueueController::failedCount() const {
  int count = 0;
  for (const TransferItem &item : m_items) {
    if (item.state == TransferState::Failed) {
      ++count;
    }
  }
  return count;
}

void TransferQueueController::setRefusal(const QString &message) {
  if (m_refusal == message) {
    return;
  }
  m_refusal = message;
  Q_EMIT refusalChanged();
}

bool TransferQueueController::enqueue(const QStringList &sourcePaths,
                                      const QString &destination,
                                      const QString &operationName) {
  if (operationName != QStringLiteral("copy") &&
      operationName != QStringLiteral("move")) {
    setRefusal(QStringLiteral("Only copy and move can be queued."));
    return false;
  }
  const TransferRouting routing = TransferRouter::route(sourcePaths, destination);
  if (routing.route == TransferRoute::Refuse) {
    setRefusal(routing.message);
    return false;
  }
  // AGENT-GUARD: the queue runs network transfers only. A request the router
  // gave to MutationController or to the ADR-0155/0156 single-child path is
  // refused here rather than quietly re-homed, so there is exactly one owner
  // per request and no path can be entered by two routes.
  if (routing.route != TransferRoute::Queue) {
    setRefusal(QStringLiteral("That transfer is not a network transfer."));
    return false;
  }
  int live = 0;
  for (const TransferItem &item : m_items) {
    if (isLive(item.state)) {
      ++live;
    }
  }
  if (live + routing.sources.size() > maximumPendingItems) {
    setRefusal(QStringLiteral("The transfer queue is full; wait for it to finish."));
    return false;
  }

  const TransferOperation operation = operationName == QStringLiteral("move")
                                          ? TransferOperation::Move
                                          : TransferOperation::Copy;
  for (const QUrl &source : routing.sources) {
    TransferItem item;
    item.id = m_nextId++;
    item.source = source;
    item.destinationFolder = routing.destinationFolder;
    item.operation = operation;
    item.state = TransferState::Queued;
    item.name = displayNameFor(source);
    m_items.append(std::move(item));
  }
  setRefusal({});
  Q_EMIT itemsChanged();
  startNext();
  return true;
}

QString TransferQueueController::routeName(const QStringList &sourcePaths,
                                          const QString &destination) const {
  switch (TransferRouter::route(sourcePaths, destination).route) {
  case TransferRoute::LocalMutation:
    return QStringLiteral("local");
  case TransferRoute::RemoteSameAuthorityChild:
    return QStringLiteral("remote-child");
  case TransferRoute::Queue:
    return QStringLiteral("queue");
  case TransferRoute::Refuse:
    break;
  }
  return QStringLiteral("refuse");
}

void TransferQueueController::startNext() {
  if (m_activeId != 0 || !m_worker) {
    return;
  }
  for (TransferItem &item : m_items) {
    if (item.state != TransferState::Queued) {
      continue;
    }
    item.state = TransferState::Running;
    item.percent = 0;
    m_activeId = item.id;
    const quint64 id = item.id;
    const QUrl source = item.source;
    const QUrl destination = item.destinationFolder;
    const TransferOperation operation = item.operation;
    // AGENT-GUARD: every field is copied above, before this signal and before
    // the worker call. Either can re-enter the queue -- a QML binding reacting
    // to itemsChanged, or a worker answering start() synchronously with an
    // immediate policy refusal -- and reallocate m_items, so the loop must
    // touch neither `item` nor its iterator afterwards.
    Q_EMIT itemsChanged();
    m_worker->start(id, source, destination, operation);
    return;
  }
}

void TransferQueueController::onWorkerProgress(const quint64 id, const int percent) {
  const int index = indexOf(id);
  if (index < 0 || m_items.at(index).state != TransferState::Running) {
    return;
  }
  m_items[index].percent = qBound(0, percent, 100);
  Q_EMIT itemsChanged();
}

void TransferQueueController::onWorkerFinished(const quint64 id,
                                               const QString &diagnostic) {
  const int index = indexOf(id);
  // AGENT-GUARD: the fence. A result for an item the user already cancelled
  // (or for an id that is no longer in the list) changes nothing.
  if (index < 0 || !isLive(m_items.at(index).state)) {
    if (m_activeId == id) {
      m_activeId = 0;
      startNext();
    }
    return;
  }
  TransferItem &item = m_items[index];
  if (diagnostic.isEmpty()) {
    item.state = TransferState::Succeeded;
    item.percent = 100;
    item.diagnostic.clear();
  } else {
    item.state = TransferState::Failed;
    item.diagnostic = NetworkLocation::boundedDiagnostic(diagnostic);
  }
  const bool succeeded = item.state == TransferState::Succeeded;
  const QUrl destination = item.destinationFolder;
  if (m_activeId == id) {
    m_activeId = 0;
  }
  trimFinished();
  Q_EMIT itemsChanged();
  if (succeeded) {
    Q_EMIT transferCommitted(destination);
  }
  startNext();
}

void TransferQueueController::pauseActive() {
  const int index = indexOf(m_activeId);
  if (index < 0 || m_items.at(index).state != TransferState::Running) {
    return;
  }
  m_items[index].state = TransferState::Paused;
  m_worker->pause(m_activeId);
  Q_EMIT itemsChanged();
}

void TransferQueueController::resumeActive() {
  const int index = indexOf(m_activeId);
  if (index < 0 || m_items.at(index).state != TransferState::Paused) {
    return;
  }
  m_items[index].state = TransferState::Running;
  m_worker->resume(m_activeId);
  Q_EMIT itemsChanged();
}

bool TransferQueueController::retire(const quint64 id) {
  const int index = indexOf(id);
  if (index < 0 || !isLive(m_items.at(index).state)) {
    return false;
  }
  m_items[index].state = TransferState::Cancelled;
  if (m_worker) {
    m_worker->cancel(id);
  }
  if (m_activeId == id) {
    m_activeId = 0;
  }
  return true;
}

void TransferQueueController::cancel(const quint64 id) {
  if (!retire(id)) {
    return;
  }
  trimFinished();
  Q_EMIT itemsChanged();
  startNext();
}

void TransferQueueController::cancelAll() {
  QVector<quint64> live;
  for (const TransferItem &item : m_items) {
    if (isLive(item.state)) {
      live.append(item.id);
    }
  }
  // AGENT-GUARD: retire every item before dispatching anything. Cancelling
  // one at a time would start the next queued transfer between cancellations
  // and hand the platform work the user just asked to stop.
  bool changed = false;
  for (const quint64 id : live) {
    changed = retire(id) || changed;
  }
  if (!changed) {
    return;
  }
  trimFinished();
  Q_EMIT itemsChanged();
  startNext();
}

void TransferQueueController::clearFinished() {
  const qsizetype before = m_items.size();
  m_items.removeIf([](const TransferItem &item) { return !isLive(item.state); });
  if (m_items.size() != before) {
    Q_EMIT itemsChanged();
  }
}

void TransferQueueController::trimFinished() {
  int finished = 0;
  for (const TransferItem &item : m_items) {
    if (!isLive(item.state)) {
      ++finished;
    }
  }
  // Drop the oldest finished entries first so the banner's history stays
  // bounded without ever discarding something still to run.
  for (int index = 0; index < m_items.size() && finished > maximumFinishedItems;) {
    if (isLive(m_items.at(index).state)) {
      ++index;
      continue;
    }
    m_items.removeAt(index);
    --finished;
  }
}

void TransferQueueController::clearRefusal() { setRefusal({}); }

} // namespace QindaQt::Apps::FileManager
