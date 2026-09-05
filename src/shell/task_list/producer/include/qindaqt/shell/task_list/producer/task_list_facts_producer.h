// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/producer/task_list_operation_authority.h"
#include "qindaqt/shell/task_list/producer/task_list_wire.h"
#include "qindaqt/shell/task_list/task_list_source.h"

#include <QByteArray>
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

// Exact-owner reader for the authenticated atomic CompositorShell1 task-fact
// snapshot. It validates the complete wire value before publishing T0 facts.
//
// AGENT-CONTRACT: A valid read is retained as (owner, epoch, revision, bytes)
// lineage. Same-owner epoch changes, revision regressions, and changed bytes
// at an equal revision fail closed. Every failure and stop changes observable
// error or availability truth and emits stateChanged. Late replies are fenced by
// (token, owner); transient transport failures use only the bounded retry
// schedule.
class TaskListFactsProducer final : public TaskListOperationAuthority {
  Q_OBJECT

public:
  explicit TaskListFactsProducer(
      TaskListProducerTransport &transport, TaskListSource &source,
      TaskListFactsProducerTiming timing = {}, QObject *parent = nullptr);
  ~TaskListFactsProducer() override;

  [[nodiscard]] bool start(QString *error = nullptr);
  void stop();

  [[nodiscard]] QString uniqueOwner() const override { return m_owner; }
  [[nodiscard]] quint64 publishedRevision() const override;
  [[nodiscard]] TaskListSourceStatus status() const override;
  [[nodiscard]] std::optional<TaskListActionGeneration>
  actionGeneration() const override;
  [[nodiscard]] std::optional<TaskListContainerLineage>
  containerLineage(const QString &containerId) const override;
  [[nodiscard]] bool refreshInFlight() const noexcept {
    return m_inFlight.has_value();
  }
  [[nodiscard]] QString lastError() const { return m_lastError; }

private Q_SLOTS:
  void handleServiceOwnerChanged(const QString &uniqueOwner);
  void handleInvalidation(const QString &uniqueOwner);
  void handleFactsRead(quint64 token, const QString &uniqueOwner,
                       const QByteArray &payload);
  void handleRefreshFailed(quint64 token, const QString &uniqueOwner,
                           const QString &message);
  void handleRefreshTimer();
  void handleRequestTimeout();

private:
  enum class TimerPurpose { None, Debounce, Retry };

  struct InFlightRefresh {
    quint64 token = 0;
    QString owner;
  };

  void scheduleDebounce(int milliseconds);
  void scheduleRetry();
  void requestNow();
  void failRefresh(const QString &message, bool permitRetry);
  void degrade(const QString &message);
  [[nodiscard]] bool factsLineageAdmits(
      const TaskListFactsResult &facts, const QByteArray &payload) const;
  [[nodiscard]] bool currentOwnerIs(const QString &uniqueOwner) const;
  void emitStateIfChanged(bool force = false);

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
  QString m_lastError;
  QString m_factsEpoch;
  QByteArray m_factsPayload;
  quint64 m_factsRevision = 0;
  std::optional<TaskListActionGeneration> m_actionGeneration;
  QVector<TaskListContainerLineage> m_containers;
  bool m_hasFactsLineage = false;
  bool m_dirty = false;
  bool m_started = false;
  QString m_signalledOwner;
  QString m_signalledError;
  quint64 m_signalledRevision = 0;
  TaskListSourceStatus m_signalledStatus = TaskListSourceStatus::Loading;
};

} // namespace QindaQt::ShellTaskList::Producer
