// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_wire.h"

#include "task_list_wire_detail.h"
#include "qindaqt/compositor/shelltaskfacts.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include <utility>

namespace QindaQt::ShellTaskList::Producer {
namespace {

using namespace Detail;

// Containers never outnumber their members, and a published container holds
// at least two windows (docs/wiki/architecture/window-containers.md).
constexpr qsizetype kMaxWireContainers = kMaxWindowFacts / 2;

bool readWindowEntry(const QJsonValue &value, TaskListWireWindow *window) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  return readIdentifier(object, QLatin1StringView("id"), false,
                        &window->windowId) &&
         readIdentifier(object, QLatin1StringView("applicationId"), false,
                        &window->applicationId) &&
         readBoundedText(object, QLatin1StringView("title"), &window->title) &&
         readIdentifier(object, QLatin1StringView("containerId"), true,
                        &window->containerId) &&
         readBoolean(object, QLatin1StringView("active"), &window->active) &&
         readBoolean(object, QLatin1StringView("minimized"),
                     &window->minimized) &&
         readBoolean(object, QLatin1StringView("skipTaskbar"),
                     &window->skipTaskbar) &&
         readBoolean(object, QLatin1StringView("skipSwitcher"),
                     &window->skipSwitcher);
}

bool readContainerEntry(const QJsonValue &value,
                        TaskListWireContainer *container) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  if (!readIdentifier(object, QLatin1StringView("id"), false,
                      &container->containerId) ||
      !readCanonicalRevision(object.value(QLatin1StringView("revision")),
                             &container->revision)) {
    return false;
  }
  const QJsonValue authority = object.value(QLatin1StringView("authority"));
  if (!authority.isString()) {
    return false;
  }
  if (authority.toString() == QLatin1StringView("control-bridge")) {
    container->authority = TaskListContainerAuthority::ControlBridge;
    return true;
  }
  if (authority.toString() == QLatin1StringView("hybrid-process")) {
    container->authority = TaskListContainerAuthority::HybridProcess;
    return true;
  }
  return false;
}

} // namespace

TaskListFactsResult TaskListWireDecoder::decodeTaskFacts(
    QByteArrayView payload) {
  TaskListFactsResult result;
  if (payload.size() > ShellVisibilityProtocol::WireLimits::MaxPayloadBytes) {
    result.error = TaskListWireError::PayloadTooLarge;
    result.message = QStringLiteral(
        "compositor task facts: payload exceeds the shell wire limit");
    return result;
  }
  QString decodeError;
  const auto snapshot = Compositor::decodeShellTaskFactsSnapshot(
      QByteArray(payload.data(), payload.size()), &decodeError);
  if (!snapshot) {
    result.error = TaskListWireError::MalformedPayload;
    result.message = decodeError;
    return result;
  }
  if (!snapshot->available()) {
    result.error = TaskListWireError::Unavailable;
    result.message = snapshot->failureCode.isEmpty()
        ? QStringLiteral("compositor task facts are unavailable")
        : snapshot->failureCode;
    return result;
  }
  result.epoch = snapshot->epoch;
  result.revision = snapshot->revision;
  result.actionGeneration = {snapshot->facts.actionGeneration.epoch,
                             snapshot->facts.actionGeneration.revision};
  result.containers.reserve(snapshot->facts.containers.size());
  for (const auto &container : snapshot->facts.containers) {
    result.containers.append({
        container.id, container.revision,
        container.authority
                == Compositor::ShellTaskContainerAuthority::ControlBridge
            ? TaskListContainerAuthority::ControlBridge
            : TaskListContainerAuthority::HybridProcess});
  }
  result.facts.reserve(snapshot->facts.windows.size());
  for (const auto &window : snapshot->facts.windows) {
    TaskWindowRole role = TaskWindowRole::Standalone;
    if (window.role == Compositor::ShellTaskWindowRole::ContainerPrimary) {
      role = TaskWindowRole::ContainerPrimary;
    } else if (window.role
               == Compositor::ShellTaskWindowRole::ContainerMember) {
      role = TaskWindowRole::ContainerMember;
    }
    const bool representative = role != TaskWindowRole::ContainerMember;
    result.facts.append({
        .windowId = window.windowId,
        .applicationId = window.applicationId,
        .applicationName = window.applicationName,
        .title = window.title,
        .outputId = window.outputId,
        .workspaceIds = window.workspaceIds,
        .onAllWorkspaces = window.onAllWorkspaces,
        .role = role,
        .containerId = window.containerId,
        .active = representative && window.active,
        .minimized = representative && window.minimized,
        .urgent = window.demandsAttention,
    });
  }
  return result;
}

TaskListWindowsResult TaskListWireDecoder::decodeWindows(
    QByteArrayView payload) {
  TaskListWindowsResult result;
  QJsonObject root;
  if (!parseRoot(payload, "compositor windows", &root, &result.error,
                 &result.message) ||
      !requireOkStatus(root, "compositor windows", &result.error,
                       &result.message)) {
    return result;
  }
  // AGENT-GUARD: Only schema 2 carries the window inventory's epoch/revision
  // fence. Older schemas cannot participate in exact lineage and are rejected.
  quint64 schemaVersion = 0;
  const QJsonValue version = root.value(QLatin1StringView("schemaVersion"));
  if (!version.isDouble() ||
      (schemaVersion = static_cast<quint64>(version.toDouble())) != 2 ||
      static_cast<double>(schemaVersion) != version.toDouble()) {
    result.error = TaskListWireError::UnsupportedSchema;
    result.message = errorMessage("compositor windows",
                                  QStringLiteral("schema version is unsupported"));
    return result;
  }
  const QJsonValue available =
      root.value(QLatin1StringView("generationAvailable"));
  if (!readIdentifier(root, QLatin1StringView("epoch"), false, &result.epoch) ||
      !isUuidShape(result.epoch) ||
      !readFenceRevision(root.value(QLatin1StringView("revision")),
                         &result.revision) ||
      !available.isBool() ||
      (available.toBool() && result.revision == 0)) {
    result.error = TaskListWireError::InvalidLineage;
    result.message = errorMessage("compositor windows",
                                  QStringLiteral("shell-action fence is invalid"));
    return result;
  }
  result.generationAvailable = available.toBool();
  const QJsonValue windows = root.value(QLatin1StringView("windows"));
  if (!windows.isArray()) {
    result.error = TaskListWireError::MalformedPayload;
    result.message = errorMessage("compositor windows",
                                  QStringLiteral("window collection is missing"));
    return result;
  }
  const QJsonArray entries = windows.toArray();
  if (entries.size() > kMaxWindowFacts) {
    result.error = TaskListWireError::LimitExceeded;
    result.message = errorMessage("compositor windows",
                                  QStringLiteral("window count exceeds the bound"));
    return result;
  }
  QSet<QString> seenIds;
  result.windows.reserve(entries.size());
  for (const QJsonValue &entry : entries) {
    TaskListWireWindow window;
    if (!readWindowEntry(entry, &window)) {
      result.error = TaskListWireError::InvalidWindow;
      result.message = errorMessage("compositor windows",
                                    QStringLiteral("window entry is invalid"));
      return result;
    }
    if (seenIds.contains(window.windowId)) {
      result.error = TaskListWireError::DuplicateWindowId;
      result.message = errorMessage("compositor windows",
                                    QStringLiteral("duplicate window id"));
      return result;
    }
    seenIds.insert(window.windowId);
    result.windows.append(std::move(window));
  }
  return result;
}

TaskListContainersResult TaskListWireDecoder::decodeContainers(
    QByteArrayView payload) {
  TaskListContainersResult result;
  QJsonObject root;
  if (!parseRoot(payload, "compositor containers", &root, &result.error,
                 &result.message) ||
      !requireOkStatus(root, "compositor containers", &result.error,
                       &result.message)) {
    return result;
  }
  const QJsonValue containers = root.value(QLatin1StringView("containers"));
  if (!containers.isArray()) {
    result.error = TaskListWireError::MalformedPayload;
    result.message = errorMessage("compositor containers",
                                  QStringLiteral("container collection is missing"));
    return result;
  }
  const QJsonArray entries = containers.toArray();
  if (entries.size() > kMaxWireContainers) {
    result.error = TaskListWireError::LimitExceeded;
    result.message = errorMessage("compositor containers",
                                  QStringLiteral("container count exceeds the bound"));
    return result;
  }
  QSet<QString> seenIds;
  result.containers.reserve(entries.size());
  for (const QJsonValue &entry : entries) {
    TaskListWireContainer container;
    if (!readContainerEntry(entry, &container)) {
      result.error = TaskListWireError::InvalidContainer;
      result.message = errorMessage("compositor containers",
                                    QStringLiteral("container entry is invalid"));
      return result;
    }
    if (seenIds.contains(container.containerId)) {
      result.error = TaskListWireError::DuplicateContainerId;
      result.message = errorMessage("compositor containers",
                                    QStringLiteral("duplicate container id"));
      return result;
    }
    seenIds.insert(container.containerId);
    result.containers.append(std::move(container));
  }
  return result;
}

} // namespace QindaQt::ShellTaskList::Producer
