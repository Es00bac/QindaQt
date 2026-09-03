// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/task_list_operation_reply.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <limits>

namespace QindaQt::ShellTaskList::Operations {
namespace {

TaskListReplyClassification verdict(TaskListReplyVerdict value, QString code,
                                    QString message) {
  TaskListReplyClassification result;
  result.verdict = value;
  result.code = std::move(code);
  result.message = std::move(message);
  return result;
}

TaskListReplyClassification lineageMismatch() {
  return verdict(TaskListReplyVerdict::UncertainLineage,
                 QStringLiteral("reply-lineage-mismatch"),
                 QStringLiteral("the compositor reply does not echo the "
                                "submitted transaction lineage; the "
                                "transaction may have committed"));
}

// Canonical decimal-string revision, matching the wire encoding.
bool parseWireRevision(const QJsonValue &value, quint64 *destination) {
  if (!value.isString()) {
    return false;
  }
  const QString text = value.toString();
  bool converted = false;
  const quint64 parsed = text.toULongLong(&converted, 10);
  if (!converted || QString::number(parsed) != text) {
    return false;
  }
  *destination = parsed;
  return true;
}

bool isInteger(const QJsonValue &value) {
  return value.isDouble() &&
         static_cast<double>(static_cast<qint64>(value.toDouble())) ==
             value.toDouble();
}

// Protocol echo: major must match exactly; a reply minor newer than 1 names
// fields this client cannot interpret.
bool protocolMatches(const QJsonObject &root) {
  const QJsonValue protocol = root.value(QLatin1StringView("protocol"));
  if (!protocol.isObject()) {
    return false;
  }
  const QJsonObject version = protocol.toObject();
  const QJsonValue major = version.value(QLatin1StringView("major"));
  const QJsonValue minor = version.value(QLatin1StringView("minor"));
  return isInteger(major) && isInteger(minor) &&
         major.toDouble() == 1.0 && minor.toDouble() >= 0.0 &&
         minor.toDouble() <= 1.0;
}

TaskListReplyClassification mapFailure(const QJsonObject &root,
                                       TaskListReplyVerdict value,
                                       const QString &defaultCode) {
  const QJsonObject failure =
      root.value(QLatin1StringView("failure")).toObject();
  const QString code =
      failure.value(QLatin1StringView("code")).toString();
  TaskListReplyClassification result =
      verdict(value, code.isEmpty() ? defaultCode : code,
              failure.value(QLatin1StringView("message")).toString());
  quint64 current = 0;
  if (value == TaskListReplyVerdict::Conflict &&
      parseWireRevision(root.value(QLatin1StringView("revision")), &current)) {
    result.currentContainerRevision = current;
    result.hasCurrentContainerRevision = true;
  }
  return result;
}

TaskListReplyClassification unknownStatus() {
  return verdict(TaskListReplyVerdict::UncertainUnknownStatus,
                 QStringLiteral("malformed-reply"),
                 QStringLiteral("the compositor reply status is unknown; the "
                                "transaction may have committed"));
}

TaskListReplyClassification classifySubmit(
    const TaskListReplyExpectation &expectation, const QJsonObject &root) {
  if (!protocolMatches(root) ||
      root.value(QLatin1StringView("transactionId")).toString() !=
          expectation.transactionId ||
      root.value(QLatin1StringView("containerId")).toString() !=
          expectation.containerId) {
    return lineageMismatch();
  }
  const QJsonValue status = root.value(QLatin1StringView("status"));
  if (!status.isString()) {
    return lineageMismatch();
  }
  quint64 revision = 0;
  if (!parseWireRevision(root.value(QLatin1StringView("revision")),
                         &revision)) {
    return lineageMismatch();
  }
  const QString outcome = status.toString();
  if (outcome == QLatin1StringView("committed")) {
    if (expectation.expectedContainerRevision ==
            std::numeric_limits<quint64>::max() ||
        revision != expectation.expectedContainerRevision + 1) {
      return lineageMismatch();
    }
    return verdict(TaskListReplyVerdict::Committed, {}, {});
  }
  if (outcome == QLatin1StringView("conflict")) {
    return mapFailure(root, TaskListReplyVerdict::Conflict,
                      QStringLiteral("revision-conflict"));
  }
  if (outcome == QLatin1StringView("rejected")) {
    return mapFailure(root, TaskListReplyVerdict::Rejected,
                      QStringLiteral("rejected"));
  }
  return unknownStatus();
}

TaskListReplyClassification classifyRelease(const QJsonObject &root) {
  const QString outcome =
      root.value(QLatin1StringView("status")).toString();
  if (outcome == QLatin1StringView("released")) {
    return verdict(TaskListReplyVerdict::Committed, {}, {});
  }
  if (outcome == QLatin1StringView("rejected")) {
    return mapFailure(root, TaskListReplyVerdict::Rejected,
                      QStringLiteral("rejected"));
  }
  return unknownStatus();
}

TaskListReplyClassification classifyDock(const QJsonObject &root) {
  // The compositor generates the dock transaction/container ids, so only
  // presence and shape are lineage here; the (token, owner) watcher binding
  // supplies exactness.
  quint64 revision = 0;
  if (!protocolMatches(root) ||
      root.value(QLatin1StringView("transactionId")).toString().isEmpty() ||
      root.value(QLatin1StringView("containerId")).toString().isEmpty() ||
      !parseWireRevision(root.value(QLatin1StringView("revision")),
                         &revision)) {
    return lineageMismatch();
  }
  const QString outcome =
      root.value(QLatin1StringView("status")).toString();
  if (outcome == QLatin1StringView("docked")) {
    // AGENT-GUARD: Compositor1 creates DockWindows staging at revision zero
    // and publishes exactly one split commit. A different success revision
    // cannot be the canonical outcome of this pending call.
    if (revision != 1) {
      return lineageMismatch();
    }
    return verdict(TaskListReplyVerdict::Committed, {}, {});
  }
  if (outcome == QLatin1StringView("rejected")) {
    return mapFailure(root, TaskListReplyVerdict::Rejected,
                      QStringLiteral("rejected"));
  }
  return unknownStatus();
}

} // namespace

TaskListReplyClassification TaskListOperationReplyCodec::classify(
    const TaskListReplyExpectation &expectation, QByteArrayView payload) {
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(
      QByteArray(payload.data(), payload.size()), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return verdict(TaskListReplyVerdict::UncertainMalformed,
                   QStringLiteral("malformed-reply"),
                   QStringLiteral("the compositor reply is not a JSON object; "
                                  "the transaction may have committed"));
  }
  const QJsonObject root = document.object();
  switch (expectation.kind) {
  case TaskListOperationKind::Submit:
    return classifySubmit(expectation, root);
  case TaskListOperationKind::Release:
    return classifyRelease(root);
  case TaskListOperationKind::Dock:
    return classifyDock(root);
  }
  return unknownStatus();
}

} // namespace QindaQt::ShellTaskList::Operations
