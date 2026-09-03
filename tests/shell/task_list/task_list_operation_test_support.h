// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"
#include "qindaqt/shell/task_list/operations/task_list_operation_transport.h"
#include "qindaqt/shell/task_list/producer/task_list_operation_authority.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

namespace TaskListOperationTest {

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskList::Producer;

class FakeOperationAuthority final : public TaskListOperationAuthority {
  Q_OBJECT

public:
  using TaskListOperationAuthority::TaskListOperationAuthority;

  [[nodiscard]] QString uniqueOwner() const override { return owner; }
  [[nodiscard]] quint64 publishedRevision() const override { return revision; }
  [[nodiscard]] TaskListSourceStatus status() const override {
    return sourceStatus;
  }
  [[nodiscard]] std::optional<TaskListContainerLineage>
  containerLineage(const QString &containerId) const override {
    for (const TaskListContainerLineage &lineage : containers) {
      if (lineage.containerId == containerId) {
        return lineage;
      }
    }
    return std::nullopt;
  }

  void setUnavailable() {
    sourceStatus = TaskListSourceStatus::Degraded;
    owner.clear();
    Q_EMIT stateChanged();
  }

  QString owner = QStringLiteral(":1.1");
  quint64 revision = 1;
  TaskListSourceStatus sourceStatus = TaskListSourceStatus::Ready;
  QVector<TaskListContainerLineage> containers{
      {QStringLiteral("c1"), 7, TaskListContainerAuthority::ControlBridge}};
};

struct RecordedCall {
  QString method;
  quint64 token = 0;
  QString owner;
  QByteArray payload;
  QStringList arguments;
};

class FakeOperationTransport final : public TaskListOperationTransport {
  Q_OBJECT

public:
  [[nodiscard]] quint64 allocateToken() override {
    if (nextToken == 0) {
      return 0;
    }
    return nextToken++;
  }

  bool submitTransaction(quint64 token, const QString &uniqueOwner,
                         const QByteArray &requestJson) override {
    calls.append({QStringLiteral("Submit"), token, uniqueOwner, requestJson,
                  {}});
    return sendSucceeds;
  }
  bool releaseContainer(quint64 token, const QString &uniqueOwner,
                        const QString &containerId) override {
    calls.append({QStringLiteral("ReleaseContainer"), token, uniqueOwner, {},
                  {containerId}});
    return sendSucceeds;
  }
  bool dockWindows(quint64 token, const QString &uniqueOwner,
                   const QString &targetWindowId,
                   const QString &incomingWindowId, const QString &orientation,
                   const QString &position, double ratio) override {
    calls.append({QStringLiteral("DockWindows"), token, uniqueOwner, {},
                  {targetWindowId, incomingWindowId, orientation, position,
                   QString::number(ratio)}});
    return sendSucceeds;
  }

  void emitReply(quint64 token, const QString &owner,
                 const QByteArray &payload) {
    Q_EMIT operationReplied(token, owner, payload);
  }
  void emitFailure(quint64 token, const QString &owner,
                   const QString &message) {
    Q_EMIT operationFailed(token, owner, message);
  }

  QVector<RecordedCall> calls;
  quint64 nextToken = 1;
  bool sendSucceeds = true;
};

struct ReadyBridgeFixture {
  FakeOperationAuthority authority;
  FakeOperationTransport operationTransport;
  TaskListOperationAdapter adapter;
  QSignalSpy finishedSpy;

  ReadyBridgeFixture()
      : adapter(authority, operationTransport, 60),
        finishedSpy(&adapter, &TaskListOperationAdapter::operationFinished) {}

  void makeReady() const {
    QCOMPARE(authority.status(), TaskListSourceStatus::Ready);
  }

  quint64 revision() const { return authority.revision; }
  QString owner() const { return authority.owner; }
};

inline TaskListOperationResult firstResult(const QSignalSpy &spy) {
  return spy.constFirst().constFirst().value<TaskListOperationResult>();
}

inline QByteArray submitReply(const QByteArray &sentRequest,
                              const QString &status, const QString &revision,
                              const QString &containerOverride = {},
                              const QString &transactionOverride = {}) {
  const QJsonObject sent = QJsonDocument::fromJson(sentRequest).object();
  return QJsonDocument(
             QJsonObject{{QStringLiteral("protocol"),
                          QJsonObject{{QStringLiteral("major"), 1},
                                      {QStringLiteral("minor"), 1}}},
                         {QStringLiteral("transactionId"),
                          transactionOverride.isNull()
                              ? sent.value(QStringLiteral("transactionId"))
                                    .toString()
                              : transactionOverride},
                         {QStringLiteral("containerId"),
                          containerOverride.isNull()
                              ? sent.value(QStringLiteral("containerId"))
                                    .toString()
                              : containerOverride},
                         {QStringLiteral("status"), status},
                         {QStringLiteral("revision"), revision}})
      .toJson(QJsonDocument::Compact);
}

} // namespace TaskListOperationTest
