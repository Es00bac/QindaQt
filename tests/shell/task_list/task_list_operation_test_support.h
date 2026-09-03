// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/operations/task_list_operation_transport.h"
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"
#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"

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

} // namespace TaskListOperationTest
