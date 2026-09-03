// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_wire.h"

#include "task_list_wire_detail.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include <cmath>
#include <utility>

namespace QindaQt::ShellTaskList::Producer {
namespace {

using namespace Detail;
using ShellVisibilityProtocol::WireLimits;

// One declared output of the snapshot's Outputs generation. Only identity and
// shape are validated; the producer consumes the id set for membership.
bool readOutputEntry(const QJsonValue &value, QString *outputId) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  if (!readIdentifier(object, QLatin1StringView("id"), false, outputId)) {
    return false;
  }
  const QJsonValue geometry = object.value(QLatin1StringView("geometry"));
  if (!geometry.isObject()) {
    return false;
  }
  const QJsonObject rect = geometry.toObject();
  const QJsonValue width = rect.value(QLatin1StringView("width"));
  const QJsonValue height = rect.value(QLatin1StringView("height"));
  if (!rect.value(QLatin1StringView("x")).isDouble() ||
      !rect.value(QLatin1StringView("y")).isDouble() || !width.isDouble() ||
      !height.isDouble() || width.toDouble() <= 0.0 ||
      height.toDouble() <= 0.0) {
    return false;
  }
  const QJsonValue scale = object.value(QLatin1StringView("scale"));
  return scale.isDouble() && std::isfinite(scale.toDouble()) &&
         scale.toDouble() > 0.0 &&
         scale.toDouble() <= WireLimits::MaxOutputScale;
}

bool readScopeEntry(const QJsonValue &value, TaskListWireScope *scope) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  if (!readIdentifier(object, QLatin1StringView("id"), false,
                      &scope->windowId) ||
      !readIdentifier(object, QLatin1StringView("outputId"), false,
                      &scope->outputId) ||
      !readBoolean(object, QLatin1StringView("onAllWorkspaces"),
                   &scope->onAllWorkspaces)) {
    return false;
  }
  const QJsonValue workspaces = object.value(QLatin1StringView("workspaceIds"));
  if (!workspaces.isArray()) {
    return false;
  }
  const QJsonArray entries = workspaces.toArray();
  // AGENT-GUARD: onAllWorkspaces requires an empty workspace list; accepting
  // both would make scope filtering depend on producer choice.
  if (scope->onAllWorkspaces != entries.isEmpty()) {
    return false;
  }
  if (entries.size() > WireLimits::MaxScopeMemberships) {
    return false;
  }
  QSet<QString> seen;
  for (const QJsonValue &entry : entries) {
    if (!entry.isString()) {
      return false;
    }
    const QString workspaceId = entry.toString();
    if (workspaceId.isEmpty() || workspaceId.size() > kMaxIdLength ||
        !isCleanText(workspaceId) || seen.contains(workspaceId)) {
      return false;
    }
    seen.insert(workspaceId);
    scope->workspaceIds.append(workspaceId);
  }
  return true;
}

} // namespace

TaskListScopeResult TaskListWireDecoder::decodeScopeSnapshot(
    QByteArrayView payload) {
  TaskListScopeResult result;
  QJsonObject root;
  if (!parseRoot(payload, "compositor scope snapshot", &root, &result.error,
                 &result.message) ||
      !requireOkStatus(root, "compositor scope snapshot", &result.error,
                       &result.message)) {
    return result;
  }
  quint64 schemaVersion = 0;
  const QJsonValue version = root.value(QLatin1StringView("schemaVersion"));
  if (!version.isDouble() ||
      (schemaVersion = static_cast<quint64>(version.toDouble())) != 1 ||
      static_cast<double>(schemaVersion) != version.toDouble()) {
    result.error = TaskListWireError::UnsupportedSchema;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("schema version is unsupported"));
    return result;
  }
  if (!readIdentifier(root, QLatin1StringView("epoch"), false,
                      &result.snapshot.epoch) ||
      !isUuidShape(result.snapshot.epoch) ||
      !readCanonicalRevision(root.value(QLatin1StringView("revision")),
                             &result.snapshot.revision)) {
    result.error = TaskListWireError::InvalidLineage;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("epoch or revision is invalid"));
    return result;
  }
  // AGENT-GUARD: Complete-snapshot validation (compositor-control-v1.md): the
  // sampled Outputs generation, the workspace/activity scope, and the outputs
  // the windows reference are all required. An incomplete or foreign-output
  // payload rejects the whole snapshot; partial scope truth is never joined.
  quint64 outputGeneration = 0;
  if (!readCanonicalRevision(root.value(QLatin1StringView("outputGeneration")),
                             &outputGeneration)) {
    result.error = TaskListWireError::InvalidLineage;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("output generation is invalid"));
    return result;
  }
  const QJsonValue scopeSelector = root.value(QLatin1StringView("scope"));
  QString scopeWorkspaceId;
  QString scopeActivityId;
  if (!scopeSelector.isObject() ||
      !readIdentifier(scopeSelector.toObject(),
                      QLatin1StringView("workspaceId"), false,
                      &scopeWorkspaceId) ||
      !readIdentifier(scopeSelector.toObject(),
                      QLatin1StringView("activityId"), false,
                      &scopeActivityId)) {
    result.error = TaskListWireError::InvalidScope;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("scope selector is invalid"));
    return result;
  }
  const QJsonValue outputs = root.value(QLatin1StringView("outputs"));
  if (!outputs.isArray()) {
    result.error = TaskListWireError::InvalidScope;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("output collection is missing"));
    return result;
  }
  const QJsonArray outputEntries = outputs.toArray();
  if (outputEntries.size() > WireLimits::MaxOutputs) {
    result.error = TaskListWireError::LimitExceeded;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("output count exceeds the bound"));
    return result;
  }
  QSet<QString> outputIds;
  for (const QJsonValue &entry : outputEntries) {
    QString outputId;
    if (!readOutputEntry(entry, &outputId) || outputIds.contains(outputId)) {
      result.error = TaskListWireError::InvalidScope;
      result.message = errorMessage("compositor scope snapshot",
                                    QStringLiteral("output entry is invalid"));
      return result;
    }
    outputIds.insert(outputId);
  }
  const QJsonValue windows = root.value(QLatin1StringView("windows"));
  if (!windows.isArray()) {
    result.error = TaskListWireError::MalformedPayload;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("window collection is missing"));
    return result;
  }
  const QJsonArray entries = windows.toArray();
  if (entries.size() > WireLimits::MaxWindows) {
    result.error = TaskListWireError::LimitExceeded;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("window count exceeds the bound"));
    return result;
  }
  QSet<QString> seenIds;
  result.snapshot.scopes.reserve(entries.size());
  for (const QJsonValue &entry : entries) {
    TaskListWireScope scope;
    if (!readScopeEntry(entry, &scope)) {
      result.error = TaskListWireError::InvalidScope;
      result.message = errorMessage("compositor scope snapshot",
                                    QStringLiteral("window scope entry is invalid"));
      return result;
    }
    if (seenIds.contains(scope.windowId)) {
      result.error = TaskListWireError::DuplicateScopeId;
      result.message = errorMessage("compositor scope snapshot",
                                    QStringLiteral("duplicate scope window id"));
      return result;
    }
    if (!outputIds.contains(scope.outputId)) {
      result.error = TaskListWireError::InvalidScope;
      result.message = errorMessage(
          "compositor scope snapshot",
          QStringLiteral("window names an output the snapshot does not declare"));
      return result;
    }
    seenIds.insert(scope.windowId);
    result.snapshot.scopes.append(std::move(scope));
  }
  return result;
}

} // namespace QindaQt::ShellTaskList::Producer
