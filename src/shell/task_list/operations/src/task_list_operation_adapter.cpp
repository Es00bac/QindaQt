// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"

#include "qindaqt/shell/task_list/operations/task_list_operation_reply.h"
#include "qindaqt/shell/task_list/operations/task_list_operation_transport.h"
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <atomic>
#include <cmath>
#include <utility>

namespace QindaQt::ShellTaskList::Operations {
namespace {

using Producer::TaskListContainerAuthority;

// Compositor1 Submit/DockWindows/ReleaseContainer identifier bound
// (docs/wiki/reference/compositor-control-v1.md, Capabilities.limits).
constexpr qsizetype kMaxOperationIdentifierLength = 256;
constexpr int kDefaultReplyTimeoutMilliseconds = 5000;
constexpr int kMaximumReplyTimeoutMilliseconds = 60'000;
constexpr quint64 kMaxInstanceOrdinal = 0xFFFFFFFFULL;
constexpr quint64 kMaxTokenSequence = 0xFFFFFFFFULL;

// AGENT-NOTE: Tokens embed a process-wide adapter-lifetime ordinal so a late
// reply from a destroyed adapter — whose pending calls the transport may
// still retain — can never match a reconstructed adapter's in-flight request
// (review finding P1-3 on candidate 3a5ae17). The counter is a monotonic
// lineage nonce, not shared mutable state: it is only ever incremented.
quint64 nextInstanceOrdinal() {
  static std::atomic<quint64> ordinal{0};
  const quint64 value = ordinal.fetch_add(1, std::memory_order_relaxed) + 1;
  return value <= kMaxInstanceOrdinal ? value : 0;
}

bool validOperationIdentifier(const QString &value) {
  return !value.isEmpty() && value.size() <= kMaxOperationIdentifierLength;
}

TaskListOperationResult makeResult(quint64 token,
                                   TaskListOperationStatus status, QString code,
                                   QString message) {
  TaskListOperationResult result;
  result.token = token;
  result.status = status;
  result.code = std::move(code);
  result.message = std::move(message);
  return result;
}

QByteArray submitRequestJson(const QString &transactionId,
                             const QString &containerId,
                             quint64 expectedContainerRevision,
                             const QJsonObject &operation) {
  const QJsonObject request{
      {QStringLiteral("protocol"),
       QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 1}}},
      {QStringLiteral("transactionId"), transactionId},
      {QStringLiteral("containerId"), containerId},
      {QStringLiteral("expectedRevision"),
       QString::number(expectedContainerRevision)},
      {QStringLiteral("operations"), QJsonArray{operation}}};
  return QJsonDocument(request).toJson(QJsonDocument::Compact);
}

} // namespace

TaskListOperationAdapter::TaskListOperationAdapter(
    Producer::TaskListFactsProducer &producer,
    TaskListOperationTransport &transport, int replyTimeoutMilliseconds,
    QObject *parent)
    : QObject(parent),
      m_producer(producer),
      m_transport(transport),
      m_replyTimeoutMilliseconds(
          replyTimeoutMilliseconds > 0 &&
                  replyTimeoutMilliseconds <= kMaximumReplyTimeoutMilliseconds
              ? replyTimeoutMilliseconds
              : kDefaultReplyTimeoutMilliseconds),
      m_instanceOrdinal(nextInstanceOrdinal()) {
  m_replyTimeout.setSingleShot(true);
  connect(&m_replyTimeout, &QTimer::timeout, this,
          &TaskListOperationAdapter::handleReplyTimeout);
  connect(&m_transport, &TaskListOperationTransport::operationReplied, this,
          &TaskListOperationAdapter::handleReply);
  connect(&m_transport, &TaskListOperationTransport::operationFailed, this,
          &TaskListOperationAdapter::handleFailure);
  connect(&m_producer, &Producer::TaskListFactsProducer::stateChanged, this,
          &TaskListOperationAdapter::handleProducerStateChanged);
}

quint64 TaskListOperationAdapter::executeTaskIntent(
    const TaskIntentRequest &request, const TaskIntentOutcome &outcome) {
  if (!outcome.ok()) {
    return finishImmediately(
        TaskListOperationStatus::InvalidRequest, QStringLiteral("invalid-intent"),
        QStringLiteral("the intent outcome was not accepted by the source"));
  }
  QString owner;
  const quint64 token = admit(PendingKind::Submit, outcome.primaryWindowId, {},
                              request.expectedRevision, &owner);
  if (owner.isEmpty()) {
    return token; // admission already finished with a fenced rejection
  }

  // AGENT-CONTRACT: Window-level activate/minimize/close are not Compositor1
  // operations; they live on the authenticated CompositorShell1 surface
  // (ADR-0061) and are routed by the later shell composition lane through the
  // published src/shell_window_actions_client. These finish Unavailable
  // rather than inventing a private compositor path; the codes name
  // Compositor1's missing surface.
  m_inFlight.reset();
  const bool container = outcome.entryKind == TaskEntryKind::Container;
  QString code;
  QString message;
  switch (request.kind) {
  case TaskIntentKind::Activate:
    code = container
               ? QStringLiteral("compositor-container-activate-unavailable")
               : QStringLiteral("compositor-window-activate-unavailable");
    message = container
                  ? QStringLiteral("Compositor1 1.1 exposes no container or "
                                   "member activation operation")
                  : QStringLiteral("Compositor1 1.1 exposes no window "
                                   "activation operation");
    break;
  case TaskIntentKind::Minimize:
    code = container
               ? QStringLiteral("compositor-container-minimize-unavailable")
               : QStringLiteral("compositor-window-minimize-unavailable");
    message = QStringLiteral("Compositor1 1.1 exposes no window or group "
                             "minimize operation");
    break;
  case TaskIntentKind::Close:
    code = container
               ? QStringLiteral("compositor-container-close-unavailable")
               : QStringLiteral("compositor-window-close-unavailable");
    message = container
                  ? QStringLiteral("container close policy is shell-owned; the "
                                   "Ungroup arm maps to releaseContainer")
                  : QStringLiteral("Compositor1 1.1 exposes no window close "
                                   "operation");
    break;
  }
  Q_EMIT operationFinished(
      makeResult(token, TaskListOperationStatus::Unavailable, std::move(code),
                 std::move(message)));
  return token;
}

quint64 TaskListOperationAdapter::activateContainerPage(
    const QString &containerId, const QString &pageId,
    quint64 expectedRevision) {
  QString owner;
  const quint64 token =
      admit(PendingKind::Submit, containerId, pageId, expectedRevision, &owner);
  if (owner.isEmpty()) {
    return token;
  }
  const auto lineage = m_producer.containerLineage(containerId);
  if (!lineage) {
    m_inFlight.reset();
    Q_EMIT operationFinished(makeResult(
        token, TaskListOperationStatus::UnknownContainer,
        QStringLiteral("unknown-container"),
        QStringLiteral("container is not part of the accepted generation")));
    return token;
  }
  if (lineage->authority != TaskListContainerAuthority::ControlBridge) {
    m_inFlight.reset();
    Q_EMIT operationFinished(makeResult(
        token, TaskListOperationStatus::UnsupportedAuthority,
        QStringLiteral("unsupported-authority"),
        QStringLiteral("Submit mutates control-bridge containers only")));
    return token;
  }
  const QString transactionId = QStringLiteral("tasklist-%1").arg(token);
  const QByteArray payload = submitRequestJson(
      transactionId, containerId, lineage->revision,
      QJsonObject{{QStringLiteral("type"), QStringLiteral("activate-page")},
                  {QStringLiteral("pageId"), pageId}});
  m_inFlight->transactionId = transactionId;
  m_inFlight->containerId = containerId;
  m_inFlight->expectedContainerRevision = lineage->revision;
  if (!m_transport.submitTransaction(token, owner, payload)) {
    finishInFlight(makeResult(token, TaskListOperationStatus::TransportFailure,
                              QStringLiteral("transport-unavailable"),
                              QStringLiteral("submit never reached the bus")));
    return token;
  }
  m_replyTimeout.start(m_replyTimeoutMilliseconds);
  return token;
}

quint64 TaskListOperationAdapter::detachWindow(const QString &containerId,
                                               const QString &windowId,
                                               quint64 expectedRevision) {
  QString owner;
  const quint64 token = admit(PendingKind::Submit, containerId, windowId,
                              expectedRevision, &owner);
  if (owner.isEmpty()) {
    return token;
  }
  const auto lineage = m_producer.containerLineage(containerId);
  if (!lineage) {
    m_inFlight.reset();
    Q_EMIT operationFinished(makeResult(
        token, TaskListOperationStatus::UnknownContainer,
        QStringLiteral("unknown-container"),
        QStringLiteral("container is not part of the accepted generation")));
    return token;
  }
  if (lineage->authority != TaskListContainerAuthority::ControlBridge) {
    m_inFlight.reset();
    Q_EMIT operationFinished(makeResult(
        token, TaskListOperationStatus::UnsupportedAuthority,
        QStringLiteral("unsupported-authority"),
        QStringLiteral("Submit mutates control-bridge containers only")));
    return token;
  }
  const QString transactionId = QStringLiteral("tasklist-%1").arg(token);
  const QByteArray payload = submitRequestJson(
      transactionId, containerId, lineage->revision,
      QJsonObject{{QStringLiteral("type"), QStringLiteral("detach-window")},
                  {QStringLiteral("windowId"), windowId}});
  m_inFlight->transactionId = transactionId;
  m_inFlight->containerId = containerId;
  m_inFlight->expectedContainerRevision = lineage->revision;
  if (!m_transport.submitTransaction(token, owner, payload)) {
    finishInFlight(makeResult(token, TaskListOperationStatus::TransportFailure,
                              QStringLiteral("transport-unavailable"),
                              QStringLiteral("submit never reached the bus")));
    return token;
  }
  m_replyTimeout.start(m_replyTimeoutMilliseconds);
  return token;
}

quint64 TaskListOperationAdapter::releaseContainer(
    const QString &containerId, quint64 expectedRevision) {
  QString owner;
  const quint64 token =
      admit(PendingKind::Release, containerId, {}, expectedRevision, &owner);
  if (owner.isEmpty()) {
    return token;
  }
  const auto lineage = m_producer.containerLineage(containerId);
  if (!lineage) {
    m_inFlight.reset();
    Q_EMIT operationFinished(makeResult(
        token, TaskListOperationStatus::UnknownContainer,
        QStringLiteral("unknown-container"),
        QStringLiteral("container is not part of the accepted generation")));
    return token;
  }
  if (lineage->authority != TaskListContainerAuthority::ControlBridge) {
    m_inFlight.reset();
    Q_EMIT operationFinished(makeResult(
        token, TaskListOperationStatus::UnsupportedAuthority,
        QStringLiteral("unsupported-authority"),
        QStringLiteral("ReleaseContainer removes control-bridge containers "
                       "only")));
    return token;
  }
  m_inFlight->containerId = containerId;
  m_inFlight->expectedContainerRevision = lineage->revision;
  if (!m_transport.releaseContainer(token, owner, containerId)) {
    finishInFlight(makeResult(token, TaskListOperationStatus::TransportFailure,
                              QStringLiteral("transport-unavailable"),
                              QStringLiteral("release never reached the bus")));
    return token;
  }
  m_replyTimeout.start(m_replyTimeoutMilliseconds);
  return token;
}

quint64 TaskListOperationAdapter::dockWindows(
    const QString &targetWindowId, const QString &incomingWindowId,
    const QString &orientation, const QString &position, double ratio,
    quint64 expectedRevision) {
  if (orientation != QLatin1StringView("horizontal") &&
      orientation != QLatin1StringView("vertical")) {
    return finishImmediately(TaskListOperationStatus::InvalidRequest,
                             QStringLiteral("malformed-dock-request"),
                             QStringLiteral("orientation must be horizontal or "
                                            "vertical"));
  }
  if (position != QLatin1StringView("first") &&
      position != QLatin1StringView("second")) {
    return finishImmediately(TaskListOperationStatus::InvalidRequest,
                             QStringLiteral("malformed-dock-request"),
                             QStringLiteral("position must be first or second"));
  }
  if (!std::isfinite(ratio) || ratio <= 0.0 || ratio >= 1.0) {
    return finishImmediately(TaskListOperationStatus::InvalidRequest,
                             QStringLiteral("malformed-dock-request"),
                             QStringLiteral("ratio must be finite and strictly "
                                            "between zero and one"));
  }
  QString owner;
  const quint64 token = admit(PendingKind::Dock, targetWindowId,
                              incomingWindowId, expectedRevision, &owner);
  if (owner.isEmpty()) {
    return token;
  }
  if (targetWindowId == incomingWindowId) {
    m_inFlight.reset();
    Q_EMIT operationFinished(makeResult(
        token, TaskListOperationStatus::InvalidRequest,
        QStringLiteral("malformed-dock-request"),
        QStringLiteral("target and incoming windows must differ")));
    return token;
  }
  if (!m_transport.dockWindows(token, owner, targetWindowId, incomingWindowId,
                               orientation, position, ratio)) {
    finishInFlight(makeResult(token, TaskListOperationStatus::TransportFailure,
                              QStringLiteral("transport-unavailable"),
                              QStringLiteral("dock never reached the bus")));
    return token;
  }
  m_replyTimeout.start(m_replyTimeoutMilliseconds);
  return token;
}

void TaskListOperationAdapter::handleReply(quint64 token,
                                           const QString &uniqueOwner,
                                           const QByteArray &payload) {
  if (!m_inFlight || m_inFlight->token != token ||
      m_inFlight->owner != uniqueOwner) {
    return;
  }
  const InFlightOperation request = *m_inFlight;
  TaskListReplyExpectation expectation;
  expectation.kind = request.kind == PendingKind::Submit
                         ? TaskListOperationKind::Submit
                         : request.kind == PendingKind::Release
                               ? TaskListOperationKind::Release
                               : TaskListOperationKind::Dock;
  expectation.transactionId = request.transactionId;
  expectation.containerId = request.containerId;
  expectation.expectedContainerRevision = request.expectedContainerRevision;
  const TaskListReplyClassification classification =
      TaskListOperationReplyCodec::classify(expectation, payload);

  switch (classification.verdict) {
  case TaskListReplyVerdict::Committed:
    finishInFlight(makeResult(token, TaskListOperationStatus::Committed, {},
                              {}));
    return;
  case TaskListReplyVerdict::Conflict: {
    auto result = makeResult(token, TaskListOperationStatus::Conflict,
                             classification.code, classification.message);
    if (classification.hasCurrentContainerRevision) {
      result.currentContainerRevision =
          classification.currentContainerRevision;
    }
    finishInFlight(std::move(result));
    return;
  }
  case TaskListReplyVerdict::Rejected:
    finishInFlight(makeResult(token, TaskListOperationStatus::Rejected,
                              classification.code, classification.message));
    return;
  case TaskListReplyVerdict::UncertainMalformed:
  case TaskListReplyVerdict::UncertainLineage:
  case TaskListReplyVerdict::UncertainUnknownStatus:
    finishInFlight(makeResult(token, TaskListOperationStatus::Uncertain,
                              classification.code, classification.message));
    return;
  }
}

void TaskListOperationAdapter::handleFailure(quint64 token,
                                             const QString &uniqueOwner,
                                             const QString &message) {
  if (!m_inFlight || m_inFlight->token != token ||
      m_inFlight->owner != uniqueOwner) {
    return;
  }
  finishInFlight(makeResult(
      token, TaskListOperationStatus::Uncertain,
      QStringLiteral("request-outcome-unknown"),
      QStringLiteral("the request was sent but failed on the bus (%1); the "
                     "transaction may have committed")
          .arg(message)));
}

void TaskListOperationAdapter::handleReplyTimeout() {
  if (!m_inFlight) {
    return;
  }
  const quint64 token = m_inFlight->token;
  finishInFlight(makeResult(
      token, TaskListOperationStatus::Uncertain,
      QStringLiteral("reply-timeout"),
      QStringLiteral("the compositor did not reply in time; the transaction "
                     "may have committed")));
}

void TaskListOperationAdapter::handleProducerStateChanged() {
  if (!m_inFlight) {
    return;
  }
  if (m_producer.uniqueOwner() == m_inFlight->owner) {
    return;
  }
  const quint64 token = m_inFlight->token;
  finishInFlight(makeResult(
      token, TaskListOperationStatus::Uncertain,
      QStringLiteral("owner-changed-in-flight"),
      QStringLiteral("the compositor owner changed while the request was in "
                     "flight; the transaction may have committed")));
}

quint64 TaskListOperationAdapter::admit(PendingKind kind,
                                        const QString &firstIdentifier,
                                        const QString &secondIdentifier,
                                        quint64 expectedRevision,
                                        QString *admittedOwner) {
  admittedOwner->clear();
  const auto reject = [this](TaskListOperationStatus status, QString code,
                             QString message) {
    return finishImmediately(status, std::move(code), std::move(message));
  };
  if (!validOperationIdentifier(firstIdentifier) ||
      (!secondIdentifier.isNull() &&
       !validOperationIdentifier(secondIdentifier))) {
    return reject(TaskListOperationStatus::InvalidRequest,
                  QStringLiteral("malformed-request"),
                  QStringLiteral("operation identifiers must be non-empty and "
                                 "within the protocol bound"));
  }
  if (m_producer.status() != TaskListSourceStatus::Ready) {
    return reject(TaskListOperationStatus::SourceNotReady,
                  QStringLiteral("source-not-ready"),
                  QStringLiteral("the facts producer has no ready generation"));
  }
  if (expectedRevision != m_producer.publishedRevision()) {
    return reject(TaskListOperationStatus::StaleGeneration,
                  QStringLiteral("stale-generation"),
                  QStringLiteral("the displayed generation is no longer "
                                 "current"));
  }
  if (m_inFlight) {
    return reject(TaskListOperationStatus::Busy,
                  QStringLiteral("operation-busy"),
                  QStringLiteral("another compositor operation is in flight"));
  }
  const QString owner = m_producer.uniqueOwner();
  if (owner.isEmpty()) {
    return reject(TaskListOperationStatus::TransportFailure,
                  QStringLiteral("transport-unavailable"),
                  QStringLiteral("no compositor owner is bound"));
  }
  const quint64 token = nextToken();
  if (token == 0) {
    return reject(TaskListOperationStatus::TransportFailure,
                  QStringLiteral("operation-token-exhausted"),
                  QStringLiteral("operation lineage is exhausted"));
  }
  InFlightOperation operation;
  operation.token = token;
  operation.owner = owner;
  operation.kind = kind;
  m_inFlight = operation;
  *admittedOwner = owner;
  return token;
}

quint64 TaskListOperationAdapter::finishImmediately(
    TaskListOperationStatus status, QString code, QString message) {
  const quint64 token = nextToken();
  if (token == 0) {
    return 0;
  }
  Q_EMIT operationFinished(
      makeResult(token, status, std::move(code), std::move(message)));
  return token;
}

void TaskListOperationAdapter::finishInFlight(TaskListOperationResult result) {
  m_inFlight.reset();
  m_replyTimeout.stop();
  Q_EMIT operationFinished(std::move(result));
}

quint64 TaskListOperationAdapter::nextToken() {
  if (m_instanceOrdinal == 0 || m_sequence > kMaxTokenSequence) {
    return 0;
  }
  return (m_instanceOrdinal << 32) | m_sequence++;
}

} // namespace QindaQt::ShellTaskList::Operations
