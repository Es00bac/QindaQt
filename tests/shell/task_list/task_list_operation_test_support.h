// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"
#include "qindaqt/shell/task_list/operations/task_list_operation_transport.h"
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"
#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

#include "task_list_producer_test_support.h"

// Shared fakes and fixtures for the operation-adapter rows. The producer fake
// is scriptable in-process (no bus); the operation fake records every call so
// tests can prove what never reached the wire.
namespace TaskListOperationTest {

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListProducerTest;

inline TaskListFactsProducerTiming fastTiming() {
  TaskListFactsProducerTiming timing;
  timing.debounceMilliseconds = 1;
  timing.requestTimeoutMilliseconds = 80;
  timing.retryMilliseconds = {5, 10, 20};
  return timing;
}

class FakeProducerTransport final : public TaskListProducerTransport {
  Q_OBJECT

public:
  bool start(QString *error = nullptr) override {
    if (error) {
      error->clear();
    }
    return true;
  }
  void stop() override {}
  void requestRefresh(quint64 token, const QString &uniqueOwner) override {
    lastToken = token;
    lastOwner = uniqueOwner;
  }

  void publishStandardScene(const QString &owner) {
    const StandardScene scene = standardScene();
    Q_EMIT windowsRead(lastToken, owner, scene.windows);
    Q_EMIT containersRead(lastToken, owner, scene.containers);
    Q_EMIT scopeRead(lastToken, owner, scene.scope);
  }

  void publishBridgeScene(const QString &owner) {
    // A control-bridge container admits Submit/ReleaseContainer; the shared
    // standard scene is hybrid-authority and would pre-reject those paths.
    const QByteArray windows = windowsPayload(
        {windowJson(QStringLiteral("w1"), QStringLiteral("app.one")),
         windowJson(QStringLiteral("w2"), QStringLiteral("app.two"),
                    QStringLiteral("c1")),
         windowJson(QStringLiteral("w3"), QStringLiteral("app.three"),
                    QStringLiteral("c1"), false, true, true)});
    const QByteArray containers = containersPayload(
        {{QStringLiteral("c1"), 7, QStringLiteral("control-bridge")}});
    const QByteArray scope = scopePayload(
        {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                        {QStringLiteral("ws-1")}),
         scopeEntryJson(QStringLiteral("w2"), QStringLiteral("output-1"),
                        {QStringLiteral("ws-1")}),
         scopeEntryJson(QStringLiteral("w3"), QStringLiteral("output-1"),
                        {QStringLiteral("ws-1")})});
    Q_EMIT windowsRead(lastToken, owner, windows);
    Q_EMIT containersRead(lastToken, owner, containers);
    Q_EMIT scopeRead(lastToken, owner, scope);
  }

  quint64 lastToken = 0;
  QString lastOwner;
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
  bool sendSucceeds = true;
};

// Drives a producer to Ready with the standard scene under the given owner.
inline void makeReady(TaskListFactsProducer &producer,
                      FakeProducerTransport &transport,
                      const QString &owner = QStringLiteral(":1.1")) {
  QVERIFY(producer.start());
  Q_EMIT transport.serviceOwnerChanged(owner);
  QTRY_VERIFY_WITH_TIMEOUT(transport.lastToken != 0, 2'000);
  transport.publishStandardScene(owner);
  QCOMPARE(producer.status(), TaskListSourceStatus::Ready);
}

inline TaskIntentOutcome acceptIntent(TaskListSource &source,
                                      const QString &taskId,
                                      TaskIntentKind kind) {
  // Fixture plumbing: callers only ask for entries the standard scene
  // contains at the current revision.
  return source.requestIntent({taskId, kind, source.revision()});
}

// Producer at Ready with the control-bridge scene plus an adapter and a
// finished-signal spy, for the reply-mapping and lineage rows.
struct ReadyBridgeFixture {
  TaskListSource source;
  FakeProducerTransport producerTransport;
  FakeOperationTransport operationTransport;
  TaskListFactsProducer producer;
  TaskListOperationAdapter adapter;
  QSignalSpy finishedSpy;

  ReadyBridgeFixture()
      : producer(producerTransport, source, fastTiming()),
        adapter(producer, operationTransport, 60),
        finishedSpy(&adapter, &TaskListOperationAdapter::operationFinished) {
    if (!producer.start()) {
      qFatal("fixture producer did not start");
    }
    Q_EMIT producerTransport.serviceOwnerChanged(QStringLiteral(":1.1"));
  }

  void makeReady() {
    QTRY_VERIFY_WITH_TIMEOUT(producerTransport.lastToken != 0, 2'000);
    producerTransport.publishBridgeScene(QStringLiteral(":1.1"));
    if (source.status() != TaskListSourceStatus::Ready) {
      qFatal("fixture scene did not reach Ready");
    }
  }

  quint64 revision() const { return source.revision(); }
  QString owner() const { return QStringLiteral(":1.1"); }
};

inline TaskListOperationResult firstResult(const QSignalSpy &spy) {
  return spy.constFirst().constFirst().value<TaskListOperationResult>();
}

// Builds the canonical Compositor1 Submit reply (replyToJson shape) echoing
// the lineage of the recorded request.
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
