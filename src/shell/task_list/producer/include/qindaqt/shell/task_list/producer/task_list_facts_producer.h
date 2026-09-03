// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/producer/task_list_fact_joiner.h"
#include "qindaqt/shell/task_list/task_list_source.h"

#include <QObject>
#include <QTimer>

#include <optional>

namespace QindaQt::ShellTaskList::Producer {

class TaskListProducerTransport;

struct TaskListFactsProducerTiming {
  int debounceMilliseconds = 16;
  int requestTimeoutMilliseconds = 2000;
  QVector<int> retryMilliseconds = {100, 250, 500, 1000, 2000, 5000};

  [[nodiscard]] bool isValid() const noexcept;
};

// Owner-lineage client that turns the public Compositor1 reads into T0 fact
// generations. It owns refresh coalescing, read timeouts, bounded retry, and
// generation fencing; it never knows QDBus types, so it qualifies against a
// deterministic fake transport.
//
// AGENT-CONTRACT: The injected TaskListSource is the only publication target.
// A refresh is accepted only when all three reads returned under the current
// owner with the in-flight token and no invalidation signal raced the join;
// anything else leaves the source Degraded with its last accepted generation
// retained (docs/wiki/shell/task-list.md). Late replies are fenced by
// (token, owner); there is no polling beyond the debounce/retry timers.
class TaskListFactsProducer final : public QObject {
  Q_OBJECT

public:
  explicit TaskListFactsProducer(
      TaskListProducerTransport &transport, TaskListSource &source,
      TaskListFactsProducerTiming timing = {}, QObject *parent = nullptr);
  ~TaskListFactsProducer() override;

  [[nodiscard]] bool start(QString *error = nullptr);
  void stop();

  [[nodiscard]] QString uniqueOwner() const { return m_owner; }
  // The source's current generation revision; 0 while Loading. The operation
  // adapter fences intents against this exact value.
  [[nodiscard]] quint64 publishedRevision() const;
  [[nodiscard]] TaskListSourceStatus status() const;
  // Lineage of one container in the last accepted generation.
  [[nodiscard]] std::optional<TaskListContainerLineage>
  containerLineage(const QString &containerId) const;
  [[nodiscard]] bool refreshInFlight() const noexcept {
    return m_inFlight.has_value();
  }
  [[nodiscard]] QString lastError() const { return m_lastError; }

Q_SIGNALS:
  // Emitted when the owner, source status, generation revision, or container
  // lineage changed. Consumers re-read the accessors; nothing is replayed.
  void stateChanged();

private Q_SLOTS:
  void handleServiceOwnerChanged(const QString &uniqueOwner);
  void handleInvalidation(const QString &uniqueOwner);
  void handleWindowsRead(quint64 token, const QString &uniqueOwner,
                         const QByteArray &payload);
  void handleContainersRead(quint64 token, const QString &uniqueOwner,
                            const QByteArray &payload);
  void handleScopeRead(quint64 token, const QString &uniqueOwner,
                       const QByteArray &payload);
  void handleRefreshFailed(quint64 token, const QString &uniqueOwner,
                           const QString &message);
  void handleRefreshTimer();
  void handleRequestTimeout();

private:
  enum class TimerPurpose {
    None,
    Debounce,
    Retry,
  };

  struct InFlightRefresh {
    quint64 token = 0;
    QString owner;
    std::optional<TaskListWindowsResult> windows;
    std::optional<TaskListContainersResult> containers;
    std::optional<TaskListScopeResult> scope;

    [[nodiscard]] bool complete() const noexcept {
      return windows.has_value() && containers.has_value() &&
             scope.has_value();
    }
  };

  void scheduleDebounce(int milliseconds);
  void scheduleRetry();
  void requestNow();
  void maybeFinishRefresh();
  void failRefresh(const QString &message, bool permitRetry);
  void degrade(const QString &message);
  void publishJoined(TaskListJoinResult joined);
  [[nodiscard]] bool currentOwnerIs(const QString &uniqueOwner) const;
  void emitStateIfChanged();

  TaskListProducerTransport &m_transport;
  TaskListSource &m_source;
  TaskListFactsProducerTiming m_timing;
  QTimer m_refreshTimer;
  QTimer m_requestTimeout;
  std::optional<InFlightRefresh> m_inFlight;
  TimerPurpose m_timerPurpose = TimerPurpose::None;
  qsizetype m_retryIndex = 0;
  quint64 m_nextToken = 1;
  QString m_owner;
  QVector<TaskListContainerLineage> m_lineage;
  QString m_lastError;
  bool m_dirty = false;
  bool m_started = false;
  // Last-signalled observable state for change-only notification.
  QString m_signalledOwner;
  quint64 m_signalledRevision = 0;
  TaskListSourceStatus m_signalledStatus = TaskListSourceStatus::Loading;
};

} // namespace QindaQt::ShellTaskList::Producer
