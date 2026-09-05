// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/producer/task_list_operation_authority.h"
#include "qindaqt/shell/task_list/task_list_types.h"

#include <QByteArrayView>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace QindaQt::ShellTaskList::Producer {

// AGENT-CONTRACT: TaskListFactsResult decodes the authenticated atomic
// CompositorShell1 snapshot. The Windows/Containers values below remain only
// for the legacy Compositor1 operation adapter and compatibility tests. Only
// consumed fields are represented; every consumed field is validated before
// any state changes. Payloads stay below the shared shell wire
// ceiling (WireLimits::MaxPayloadBytes) and the T0 identity bound of 512
// characters; identifiers never contain control, format, or malformed
// surrogate characters.

// One entry of the Compositor1 Windows() inventory.
struct TaskListWireWindow {
  QString windowId;
  QString applicationId;
  QString title;
  // Empty for an independent window; otherwise the owning container id.
  QString containerId;
  bool active = false;
  bool minimized = false;
  // Legacy inventory truth only; T1 publication uses TaskListFactsResult.
  bool skipTaskbar = false;
  bool skipSwitcher = false;

  friend bool operator==(const TaskListWireWindow &,
                         const TaskListWireWindow &) = default;
};

// One entry of the independent Compositor1 Containers() inventory. The decoder
// validates its authoritative decimal-string revision. The facts producer
// never joins it to Windows(); TaskListFactsResult carries current lineage.
struct TaskListWireContainer {
  QString containerId;
  quint64 revision = 0;
  TaskListContainerAuthority authority = TaskListContainerAuthority::HybridProcess;

  friend bool operator==(const TaskListWireContainer &,
                         const TaskListWireContainer &) = default;
};

enum class TaskListWireError {
  None,
  PayloadTooLarge,
  MalformedPayload,
  Unavailable,
  UnsupportedSchema,
  InvalidLineage,
  InvalidWindow,
  DuplicateWindowId,
  InvalidContainer,
  DuplicateContainerId,
  LimitExceeded,
};

struct TaskListWindowsResult {
  QVector<TaskListWireWindow> windows;
  // The schema-2 shell-action fence carried by this inventory. It is retained
  // solely as Windows lineage; the Compositor1 contract forbids using it to
  // combine the separate panel-visibility inventory into task-list facts.
  QString epoch;
  quint64 revision = 0;
  bool generationAvailable = false;
  TaskListWireError error = TaskListWireError::None;
  QString message;

  [[nodiscard]] bool ok() const noexcept {
    return error == TaskListWireError::None;
  }
};

struct TaskListContainersResult {
  QVector<TaskListWireContainer> containers;
  TaskListWireError error = TaskListWireError::None;
  QString message;

  [[nodiscard]] bool ok() const noexcept {
    return error == TaskListWireError::None;
  }
};

struct TaskListFactsResult {
  QVector<TaskWindowFact> facts;
  QVector<TaskListContainerLineage> containers;
  QString epoch;
  quint64 revision = 0;
  TaskListActionGeneration actionGeneration;
  TaskListWireError error = TaskListWireError::None;
  QString message;

  [[nodiscard]] bool ok() const noexcept {
    return error == TaskListWireError::None;
  }
};

// Stateless hostile-input decoders. Each rejects atomically.
class TaskListWireDecoder final {
public:
  [[nodiscard]] static TaskListWindowsResult decodeWindows(
      QByteArrayView payload);
  [[nodiscard]] static TaskListContainersResult decodeContainers(
      QByteArrayView payload);
  [[nodiscard]] static TaskListFactsResult decodeTaskFacts(
      QByteArrayView payload);
};

} // namespace QindaQt::ShellTaskList::Producer
