// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"

#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"

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
    : TaskListOperationAuthority(parent),
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
    const QString message =
        QStringLiteral("task-list facts producer timing is invalid");
    degrade(message);
    emitStateIfChanged(true);
    setError(error, message);
    return false;
  }
  m_started = true;
  QString transportError;
  if (!m_transport.start(&transportError)) {
    m_started = false;
    const QString message = transportError.isEmpty()
                                ? QStringLiteral("task-list transport failed")
                                : transportError;
    degrade(message);
    emitStateIfChanged(true);
    setError(error, message);
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
  m_retryIndex = 0;
  m_transport.stop();
  m_owner.clear();
  m_windowEpoch.clear();
  m_windowPayload.clear();
  m_windowRevision = 0;
  m_hasWindowLineage = false;
  // AGENT-GUARD: Stop withdraws availability even before the first accepted
  // generation. Candidate 3a5ae17 retained Ready owner/lineage and admitted a
  // mutation after stop (review finding P1-2).
  degrade(QStringLiteral("task-list facts producer stopped"));
  emitStateIfChanged(true);
}

quint64 TaskListFactsProducer::publishedRevision() const {
  return m_source.revision();
}

TaskListSourceStatus TaskListFactsProducer::status() const {
  return m_source.status();
}

std::optional<TaskListContainerLineage>
TaskListFactsProducer::containerLineage(const QString &) const {
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
  const bool clearSource = uniqueOwner.isEmpty()
      || (!m_owner.isEmpty() && m_owner != uniqueOwner);
  m_owner = uniqueOwner;
  m_windowEpoch.clear();
  m_windowPayload.clear();
  m_windowRevision = 0;
  m_hasWindowLineage = false;

  // AGENT-GUARD: owner replacement invalidates every task id and action
  // fence. Clear the retained generation before degrading; showing the old
  // owner's rows after the action authority moved would be stale UI truth.
  if (clearSource) {
    m_source.reset();
  }
  if (uniqueOwner.isEmpty()) {
    degrade(QStringLiteral("compositor owner is unavailable"));
  } else {
    degrade(QStringLiteral("compositor owner changed; coherent task-list "
                           "inventory is not yet available"));
    scheduleDebounce(0);
  }
  emitStateIfChanged(true);
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
  m_inFlight.reset();
  m_requestTimeout.stop();
  if (m_dirty) {
    m_dirty = false;
    scheduleDebounce(m_timing.debounceMilliseconds);
    return;
  }

  const TaskListWindowsResult decoded =
      TaskListWireDecoder::decodeWindows(payload);
  if (!decoded.ok()) {
    failRefresh(decoded.message, true);
    return;
  }
  if (!decoded.generationAvailable) {
    failRefresh(QStringLiteral("compositor window generation is unavailable"),
                false);
    return;
  }
  // AGENT-NOTE: P1-1 on 3a5ae17 proved that matching the visibility fence did
  // not authorize combining two inventories. This lineage validates only the
  // documented Windows() payload; no scope bytes enter this module.
  if (!windowLineageAdmits(decoded, payload)) {
    failRefresh(QStringLiteral("compositor window inventory lineage regressed "
                               "or collided"),
                false);
    return;
  }
  m_windowEpoch = decoded.epoch;
  m_windowRevision = decoded.revision;
  m_windowPayload = payload;
  m_hasWindowLineage = true;
  m_retryIndex = 0;

  // Windows() lacks the required output/workspace and atomic container
  // revision facts. Publishing placeholders would turn missing authority into
  // UI truth, so the current public protocol is intentionally unavailable.
  failRefresh(QStringLiteral("Compositor1 has no coherent task-list facts "
                             "inventory"),
              false);
}

void TaskListFactsProducer::handleRefreshFailed(
    quint64 token, const QString &uniqueOwner, const QString &message) {
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
  if (!m_started || m_inFlight || m_owner.isEmpty()) {
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
  m_inFlight = InFlightRefresh{token, m_owner};
  m_requestTimeout.start(m_timing.requestTimeoutMilliseconds);
  m_transport.requestRefresh(token, m_owner);
}

void TaskListFactsProducer::failRefresh(const QString &message,
                                        bool permitRetry) {
  const bool followUp = m_dirty;
  m_dirty = false;
  m_inFlight.reset();
  m_requestTimeout.stop();
  degrade(message);
  // AGENT-GUARD: The error is part of observable availability. Include it in
  // change detection so every distinct failure and stop path notifies even if
  // the source was already Degraded (review finding P1-2).
  emitStateIfChanged(true);
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
  m_lastError = message.trimmed().isEmpty()
                    ? QStringLiteral("compositor task-list refresh failed")
                    : message;
  m_source.markDegraded();
}

bool TaskListFactsProducer::windowLineageAdmits(
    const TaskListWindowsResult &windows, const QByteArray &payload) const {
  if (!m_hasWindowLineage) {
    return true;
  }
  if (windows.epoch != m_windowEpoch) {
    return false;
  }
  if (windows.revision > m_windowRevision) {
    return true;
  }
  return windows.revision == m_windowRevision && payload == m_windowPayload;
}

bool TaskListFactsProducer::currentOwnerIs(const QString &uniqueOwner) const {
  return !m_owner.isEmpty() && m_owner == uniqueOwner;
}

void TaskListFactsProducer::emitStateIfChanged(bool force) {
  const quint64 revision = m_source.revision();
  const TaskListSourceStatus sourceStatus = m_source.status();
  if (!force && m_signalledOwner == m_owner && m_signalledRevision == revision &&
      m_signalledStatus == sourceStatus && m_signalledError == m_lastError) {
    return;
  }
  m_signalledOwner = m_owner;
  m_signalledRevision = revision;
  m_signalledStatus = sourceStatus;
  m_signalledError = m_lastError;
  Q_EMIT stateChanged();
}

} // namespace QindaQt::ShellTaskList::Producer
