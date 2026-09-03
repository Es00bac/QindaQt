// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArrayView>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace QindaQt::ShellTaskList::Producer {

// AGENT-CONTRACT: These values are the shell-side decoding of the public
// org.qindaqt.Compositor1 authority (compositor/dbus/org.qindaqt.Compositor1.xml,
// docs/wiki/reference/compositor-control-v1.md). Only fields the task list
// consumes are represented; every consumed field is validated here so the
// joiner never sees hostile values. Payloads stay below the shared shell wire
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
  // AGENT-NOTE: The compositor's collapsed native identity gives every
  // container exactly one member with skipTaskbar == false; the joiner
  // classifies that member as the primary. A standalone window with
  // skipTaskbar == true opted out of task lists and is dropped by the joiner.
  bool skipTaskbar = false;
  bool skipSwitcher = false;

  friend bool operator==(const TaskListWireWindow &,
                         const TaskListWireWindow &) = default;
};

enum class TaskListContainerAuthority {
  ControlBridge,
  HybridProcess,
};

// One entry of the Compositor1 Containers() inventory. revision is the
// authoritative decimal-string container revision, retained so the operation
// adapter can fence Submit transactions with expectedRevision.
struct TaskListWireContainer {
  QString containerId;
  quint64 revision = 0;
  TaskListContainerAuthority authority = TaskListContainerAuthority::HybridProcess;

  friend bool operator==(const TaskListWireContainer &,
                         const TaskListWireContainer &) = default;
};

// The task-list projection of one ShellVisibilitySnapshot window entry: output
// and workspace scope only. Windows() deliberately carries neither, so this
// read is the sole public scope authority (see docs/wiki/shell/task-list.md).
struct TaskListWireScope {
  QString windowId;
  QString outputId;
  QStringList workspaceIds;
  bool onAllWorkspaces = false;

  friend bool operator==(const TaskListWireScope &,
                         const TaskListWireScope &) = default;
};

// Decoded ShellVisibilitySnapshot lineage. epoch is validated as a UUID and
// revision as a canonical nonzero decimal string. The producer retains the
// last accepted (owner, epoch, revision, payload) and rejects regressions and
// changed bytes at an equal revision; join coherence additionally requires the
// exact schema-2 Windows() fence (TaskListWindowsResult::epoch/revision) to
// name this generation (docs/wiki/reference/compositor-control-v1.md).
struct TaskListWireScopeSnapshot {
  QString epoch;
  quint64 revision = 0;
  QVector<TaskListWireScope> scopes;

  friend bool operator==(const TaskListWireScopeSnapshot &,
                         const TaskListWireScopeSnapshot &) = default;
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
  InvalidScope,
  DuplicateScopeId,
  LimitExceeded,
};

struct TaskListWindowsResult {
  QVector<TaskListWireWindow> windows;
  // The schema-2 shell-action fence: the exact (epoch, revision) of the
  // ShellVisibilitySnapshot generation this read was sampled against, plus
  // whether such a generation currently exists. The producer may join scope
  // truth only when the scope snapshot matches this fence exactly.
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

struct TaskListScopeResult {
  TaskListWireScopeSnapshot snapshot;
  TaskListWireError error = TaskListWireError::None;
  QString message;

  [[nodiscard]] bool ok() const noexcept {
    return error == TaskListWireError::None;
  }
};

// Stateless hostile-input decoders. Each rejects atomically: a single invalid
// entry discards the whole payload so the producer can never join a partial
// inventory.
class TaskListWireDecoder final {
public:
  [[nodiscard]] static TaskListWindowsResult decodeWindows(
      QByteArrayView payload);
  [[nodiscard]] static TaskListContainersResult decodeContainers(
      QByteArrayView payload);
  [[nodiscard]] static TaskListScopeResult decodeScopeSnapshot(
      QByteArrayView payload);
};

} // namespace QindaQt::ShellTaskList::Producer
