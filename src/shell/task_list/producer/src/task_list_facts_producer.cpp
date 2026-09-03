// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"

#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"
#include "qindaqt/shell/task_list/producer/task_list_wire.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::ShellTaskList::Producer {
namespace {

constexpr int kMaximumRuntimeDelayMilliseconds = 60'000;

void setError(QString *error, QString message) {
  if (error) {
    *error = std::move(message);
  }
}

} // namespace

bool TaskListFactsProducerTiming::isValid() const noexcept {
  if (debounceMilliseconds < 0 ||
      debounceMilliseconds > kMaximumRuntimeDelayMilliseconds ||
      requestTimeoutMilliseconds <= 0 ||
      requestTimeoutMilliseconds > kMaximumRuntimeDelayMilliseconds ||
      retryMilliseconds.isEmpty()) {
    return false;
  }
  int previous = 0;
  for (const int delay : retryMilliseconds) {
    if (delay <= 0 || delay > kMaximumRuntimeDelayMilliseconds ||
        delay < previous) {
      return false;
    }
    previous = delay;
  }
  return true;
}

TaskListFactsProducer::TaskListFactsProducer(
    TaskListProducerTransport &transport, TaskListSource &source,
    TaskListFactsProducerTiming timing, QObject *parent)
    : QObject(parent),
      m_transport(transport),
      m_source(source),
      m_timing(std::move(timing)) {
  m_refreshTimer.setSingleShot(true);
  m_requestTimeout.setSingleShot(true);
  connect(&m_refreshTimer, &QTimer::timeout, this,
          &TaskListFactsProducer::handleRefreshTimer);
  connect(&m_requestTimeout, &QTimer::timeout, this,
          &TaskListFactsProducer::handleRequestTimeout);
  connect(&m_transport, &TaskListProducerTransport::serviceOwnerChanged, this,
          &TaskListFactsProducer::handleServiceOwnerChanged);
  connect(&m_transport, &TaskListProducerTransport::refreshInvalidated, this,
          &TaskListFactsProducer::handleInvalidation);
  connect(&m_transport, &TaskListProducerTransport::windowsRead, this,
          &TaskListFactsProducer::handleWindowsRead);
  connect(&m_transport, &TaskListProducerTransport::containersRead, this,
          &TaskListFactsProducer::handleContainersRead);
  connect(&m_transport, &TaskListProducerTransport::scopeRead, this,
          &TaskListFactsProducer::handleScopeRead);
  connect(&m_transport, &TaskListProducerTransport::refreshFailed, this,
          &TaskListFactsProducer::handleRefreshFailed);
}

TaskListFactsProducer::~TaskListFactsProducer() { stop(); }

bool TaskListFactsProducer::start(QString *error) {
  if (m_started) {
    setError(error, {});
    return true;
  }
  if (!m_timing.isValid()) {
    setError(error, QStringLiteral("task-list facts producer timing is invalid"));
    return false;
  }
  m_started = true;
  if (!m_transport.start(error)) {
    m_started = false;
    return false;
  }
  setError(error, {});
  return true;
}

void TaskListFactsProducer::stop() {
  if (!m_started) {
    return;
  }
  m_started = false;
  m_refreshTimer.stop();
  m_requestTimeout.stop();
  m_timerPurpose = TimerPurpose::None;
  m_inFlight.reset();
  m_dirty = false;
  m_transport.stop();
}

quint64 TaskListFactsProducer::publishedRevision() const {
  return m_source.revision();
}

TaskListSourceStatus TaskListFactsProducer::status() const {
  return m_source.status();
}

std::optional<TaskListContainerLineage>
TaskListFactsProducer::containerLineage(const QString &containerId) const {
  for (const TaskListContainerLineage &entry : m_lineage) {
    if (entry.containerId == containerId) {
      return entry;
    }
  }
  return std::nullopt;
}

void TaskListFactsProducer::handleServiceOwnerChanged(
    const QString &uniqueOwner) {
  if (!m_started) {
    return;
  }
  m_refreshTimer.stop();
  m_requestTimeout.stop();
  m_timerPurpose = TimerPurpose::None;
  m_inFlight.reset();
  m_dirty = false;
  m_retryIndex = 0;
  m_owner = uniqueOwner;

  if (uniqueOwner.isEmpty()) {
    // Owner loss keeps the last accepted generation visible but refuses every
    // intent until a fresh publish succeeds.
    degrade(QStringLiteral("compositor owner was lost"));
  } else {
    // Replacement: the retained generation is stale until the new owner
    // publishes a coherent one.
    degrade(QStringLiteral("compositor owner was replaced"));
    scheduleDebounce(0);
  }
  emitStateIfChanged();
}

void TaskListFactsProducer::handleInvalidation(const QString &uniqueOwner) {
  if (!m_started || !currentOwnerIs(uniqueOwner)) {
    return;
  }
  if (m_inFlight) {
    m_dirty = true;
    return;
  }
  scheduleDebounce(m_timing.debounceMilliseconds);
}

void TaskListFactsProducer::handleWindowsRead(quint64 token,
                                              const QString &uniqueOwner,
                                              const QByteArray &payload) {
  if (!m_started || !m_inFlight || m_inFlight->token != token ||
      m_inFlight->owner != uniqueOwner) {
    return;
  }
  auto decoded = TaskListWireDecoder::decodeWindows(payload);
  if (!decoded.ok()) {
    failRefresh(decoded.message, true);
    return;
  }
  m_inFlight->windows = std::move(decoded);
  maybeFinishRefresh();
}

void TaskListFactsProducer::handleContainersRead(quint64 token,
                                                 const QString &uniqueOwner,
                                                 const QByteArray &payload) {
  if (!m_started || !m_inFlight || m_inFlight->token != token ||
      m_inFlight->owner != uniqueOwner) {
    return;
  }
  auto decoded = TaskListWireDecoder::decodeContainers(payload);
  if (!decoded.ok()) {
    failRefresh(decoded.message, true);
    return;
  }
  m_inFlight->containers = std::move(decoded);
  maybeFinishRefresh();
}

void TaskListFactsProducer::handleScopeRead(quint64 token,
                                            const QString &uniqueOwner,
                                            const QByteArray &payload) {
  if (!m_started || !m_inFlight || m_inFlight->token != token ||
      m_inFlight->owner != uniqueOwner) {
    return;
  }
  auto decoded = TaskListWireDecoder::decodeScopeSnapshot(payload);
  if (!decoded.ok()) {
    failRefresh(decoded.message, true);
    return;
  }
  m_inFlight->scope = std::move(decoded);
  maybeFinishRefresh();
}

void TaskListFactsProducer::handleRefreshFailed(quint64 token,
                                                const QString &uniqueOwner,
                                                const QString &message) {
  if (!m_started || !m_inFlight || m_inFlight->token != token ||
      m_inFlight->owner != uniqueOwner) {
    return;
  }
  failRefresh(message, true);
}

void TaskListFactsProducer::handleRefreshTimer() {
  m_timerPurpose = TimerPurpose::None;
  requestNow();
}

void TaskListFactsProducer::handleRequestTimeout() {
  if (!m_started || !m_inFlight) {
    return;
  }
  failRefresh(QStringLiteral("compositor task-list refresh timed out"), true);
}

void TaskListFactsProducer::scheduleDebounce(int milliseconds) {
  if (!m_started || m_inFlight || m_owner.isEmpty()) {
    return;
  }
  if (m_refreshTimer.isActive() && m_timerPurpose == TimerPurpose::Debounce) {
    return;
  }
  m_refreshTimer.stop();
  m_timerPurpose = TimerPurpose::Debounce;
  m_refreshTimer.start(milliseconds);
}

void TaskListFactsProducer::scheduleRetry() {
  if (!m_started || m_inFlight || m_owner.isEmpty() ||
      m_timing.retryMilliseconds.isEmpty()) {
    return;
  }
  const qsizetype last = m_timing.retryMilliseconds.size() - 1;
  const qsizetype index = std::min(m_retryIndex, last);
  const int delay = m_timing.retryMilliseconds.at(index);
  if (m_retryIndex < last) {
    ++m_retryIndex;
  }
  m_refreshTimer.stop();
  m_timerPurpose = TimerPurpose::Retry;
  m_refreshTimer.start(delay);
}

void TaskListFactsProducer::requestNow() {
  if (!m_started || m_inFlight || m_owner.isEmpty()) {
    return;
  }
  if (m_nextToken == 0) {
    failRefresh(QStringLiteral("task-list refresh token is exhausted"), false);
    return;
  }
  const quint64 token = m_nextToken;
  m_nextToken = token == std::numeric_limits<quint64>::max() ? 0 : token + 1;
  m_inFlight = InFlightRefresh{};
  m_inFlight->token = token;
  m_inFlight->owner = m_owner;
  m_requestTimeout.start(m_timing.requestTimeoutMilliseconds);
  m_transport.requestRefresh(token, m_owner);
}

void TaskListFactsProducer::maybeFinishRefresh() {
  if (!m_inFlight || !m_inFlight->complete()) {
    return;
  }
  InFlightRefresh finished = std::move(*m_inFlight);
  m_inFlight.reset();
  m_requestTimeout.stop();

  // AGENT-GUARD: An invalidation racing the three reads means the join would
  // mix generations; discard the complete set and re-read once instead of
  // publishing a torn batch the source cannot detect.
  if (m_dirty) {
    m_dirty = false;
    scheduleDebounce(m_timing.debounceMilliseconds);
    return;
  }

  TaskListJoinResult joined = TaskListFactJoiner::join(
      finished.windows->windows, finished.containers->containers,
      finished.scope->snapshot);
  if (!joined.ok()) {
    degrade(joined.error.message);
    scheduleRetry();
    return;
  }
  publishJoined(std::move(joined));
}

void TaskListFactsProducer::failRefresh(const QString &message,
                                        bool permitRetry) {
  const bool followUp = m_dirty;
  m_dirty = false;
  m_inFlight.reset();
  m_requestTimeout.stop();
  degrade(message);
  if (!m_started) {
    return;
  }
  if (followUp) {
    scheduleDebounce(m_timing.debounceMilliseconds);
  } else if (permitRetry) {
    scheduleRetry();
  }
}

void TaskListFactsProducer::degrade(const QString &message) {
  m_lastError = message;
  m_source.markDegraded();
}

void TaskListFactsProducer::publishJoined(TaskListJoinResult joined) {
  const TaskListEvaluation evaluation =
      m_source.publishGeneration(joined.facts);
  if (!evaluation.ok()) {
    // The model re-validates producer classification (ADR-0044); a rejection
    // here means the wire join produced facts the T0 contract forbids.
    degrade(evaluation.error.message);
    scheduleRetry();
    emitStateIfChanged();
    return;
  }
  m_lineage = std::move(joined.containers);
  m_retryIndex = 0;
  m_lastError.clear();
  emitStateIfChanged();
}

bool TaskListFactsProducer::currentOwnerIs(const QString &uniqueOwner) const {
  return !m_owner.isEmpty() && m_owner == uniqueOwner;
}

void TaskListFactsProducer::emitStateIfChanged() {
  const quint64 revision = m_source.revision();
  const TaskListSourceStatus status = m_source.status();
  if (m_signalledOwner == m_owner && m_signalledRevision == revision &&
      m_signalledStatus == status) {
    return;
  }
  m_signalledOwner = m_owner;
  m_signalledRevision = revision;
  m_signalledStatus = status;
  Q_EMIT stateChanged();
}

} // namespace QindaQt::ShellTaskList::Producer
