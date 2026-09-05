// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_types.h"

#include <QObject>
#include <QString>
#include <QtTypes>

#include <optional>

namespace QindaQt::ShellTaskList::Producer {

enum class TaskListContainerAuthority {
  ControlBridge,
  HybridProcess,
};

struct TaskListContainerLineage {
  QString containerId;
  quint64 revision = 0;
  TaskListContainerAuthority authority = TaskListContainerAuthority::HybridProcess;

  friend bool operator==(const TaskListContainerLineage &,
                         const TaskListContainerLineage &) = default;
};

struct TaskListActionGeneration {
  QString epoch;
  quint64 revision = 0;

  friend bool operator==(const TaskListActionGeneration &,
                         const TaskListActionGeneration &) = default;
};

// Narrow read-only authority consumed by the mutation adapter. Production
// uses TaskListFactsProducer; tests inject deterministic state without
// fabricating a coherent Compositor1 facts generation.
//
// AGENT-CONTRACT: All accessors describe one current publication boundary on
// the object's thread. A non-Ready status or empty owner denies every
// mutation. stateChanged tells consumers to re-read the complete boundary.
class TaskListOperationAuthority : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~TaskListOperationAuthority() override = default;

  [[nodiscard]] virtual QString uniqueOwner() const = 0;
  [[nodiscard]] virtual quint64 publishedRevision() const = 0;
  [[nodiscard]] virtual TaskListSourceStatus status() const = 0;
  [[nodiscard]] virtual std::optional<TaskListActionGeneration>
  actionGeneration() const = 0;
  [[nodiscard]] virtual std::optional<TaskListContainerLineage>
  containerLineage(const QString &containerId) const = 0;

Q_SIGNALS:
  void stateChanged();
};

} // namespace QindaQt::ShellTaskList::Producer
