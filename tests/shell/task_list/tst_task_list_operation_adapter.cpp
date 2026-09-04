// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"
#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"

#include <QSignalSpy>
#include <QtTest>

#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListOperationTest;

namespace {

TaskIntentOutcome acceptedOutcome(TaskEntryKind kind, const QString &windowId) {
  TaskIntentOutcome outcome;
  outcome.entryKind = kind;
  outcome.primaryWindowId = windowId;
  outcome.memberWindowIds = {windowId};
  return outcome;
}

class IdleProducerTransport final : public TaskListProducerTransport {
public:
  using TaskListProducerTransport::TaskListProducerTransport;
  bool start(QString *error = nullptr) override {
    if (error) {
      error->clear();
    }
    return true;
  }
  void stop() override {}
  void requestRefresh(quint64, const QString &) override {}
};

} // namespace

class TaskListOperationAdapterTests final : public QObject {
  Q_OBJECT

private slots:
  void windowIntentsAreUnavailableWithCompositionCodes();
  void containerIntentsAreUnavailableWithCompositionCodes();
  void staleGenerationRejectsBeforeBusTraffic();
  void degradedSourceRejectsBeforeBusTraffic();
  void hybridAuthorityRejectsSubmitAndRelease();
  void unknownContainerIsRejected();
  void busyAdapterRejectsSecondOperation();
  void stoppedProducerAdmitsNoOperation();
};

void TaskListOperationAdapterTests::windowIntentsAreUnavailableWithCompositionCodes() {
  FakeOperationAuthority authority;
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(authority, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);
  const TaskIntentOutcome outcome =
      acceptedOutcome(TaskEntryKind::Window, QStringLiteral("w1"));

  adapter.executeTaskIntent(
      {QStringLiteral("w1"), TaskIntentKind::Activate, authority.revision},
      outcome);
  QCOMPARE(firstResult(finishedSpy).status,
           TaskListOperationStatus::Unavailable);
  QCOMPARE(firstResult(finishedSpy).code,
           QStringLiteral("compositor-window-activate-unavailable"));

  adapter.executeTaskIntent(
      {QStringLiteral("w1"), TaskIntentKind::Minimize, authority.revision},
      outcome);
  QCOMPARE(finishedSpy.at(1).constFirst().value<TaskListOperationResult>().code,
           QStringLiteral("compositor-window-minimize-unavailable"));

  adapter.executeTaskIntent(
      {QStringLiteral("w1"), TaskIntentKind::Close, authority.revision},
      outcome);
  QCOMPARE(finishedSpy.at(2).constFirst().value<TaskListOperationResult>().code,
           QStringLiteral("compositor-window-close-unavailable"));

  adapter.executeTaskIntent(
      {QStringLiteral("w1"), TaskIntentKind::Raise, authority.revision},
      outcome);
  QCOMPARE(finishedSpy.at(3).constFirst().value<TaskListOperationResult>().code,
           QStringLiteral("compositor-window-raise-unavailable"));
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::containerIntentsAreUnavailableWithCompositionCodes() {
  FakeOperationAuthority authority;
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(authority, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);
  adapter.executeTaskIntent(
      {QStringLiteral("c1"), TaskIntentKind::Activate, authority.revision},
      acceptedOutcome(TaskEntryKind::Container, QStringLiteral("w2")));
  QCOMPARE(firstResult(finishedSpy).code,
           QStringLiteral("compositor-container-activate-unavailable"));
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::staleGenerationRejectsBeforeBusTraffic() {
  FakeOperationAuthority authority;
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(authority, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);
  adapter.releaseContainer(QStringLiteral("c1"), quint64(999));
  QCOMPARE(firstResult(finishedSpy).status,
           TaskListOperationStatus::StaleGeneration);
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::degradedSourceRejectsBeforeBusTraffic() {
  FakeOperationAuthority authority;
  authority.setUnavailable();
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(authority, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);
  adapter.releaseContainer(QStringLiteral("c1"), authority.revision);
  QCOMPARE(firstResult(finishedSpy).status,
           TaskListOperationStatus::SourceNotReady);
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::hybridAuthorityRejectsSubmitAndRelease() {
  FakeOperationAuthority authority;
  authority.containers[0].authority = TaskListContainerAuthority::HybridProcess;
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(authority, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);

  adapter.activateContainerPage(QStringLiteral("c1"), QStringLiteral("page-2"),
                                authority.revision);
  adapter.releaseContainer(QStringLiteral("c1"), authority.revision);
  adapter.detachWindow(QStringLiteral("c1"), QStringLiteral("w3"),
                       authority.revision);
  QCOMPARE(finishedSpy.size(), 3);
  for (int index = 0; index < 3; ++index) {
    QCOMPARE(finishedSpy.at(index).constFirst()
                 .value<TaskListOperationResult>()
                 .status,
             TaskListOperationStatus::UnsupportedAuthority);
  }
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::unknownContainerIsRejected() {
  FakeOperationAuthority authority;
  authority.containers.clear();
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(authority, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);
  adapter.releaseContainer(QStringLiteral("c-unknown"), authority.revision);
  QCOMPARE(firstResult(finishedSpy).status,
           TaskListOperationStatus::UnknownContainer);
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::busyAdapterRejectsSecondOperation() {
  FakeOperationAuthority authority;
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(authority, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);
  adapter.dockWindows(QStringLiteral("w1"), QStringLiteral("w9"),
                      QStringLiteral("horizontal"), QStringLiteral("second"),
                      0.5, authority.revision);
  QVERIFY(adapter.operationInFlight());
  adapter.dockWindows(QStringLiteral("w1"), QStringLiteral("w8"),
                      QStringLiteral("vertical"), QStringLiteral("first"),
                      0.5, authority.revision);
  QCOMPARE(firstResult(finishedSpy).status, TaskListOperationStatus::Busy);
  QCOMPARE(operations.calls.size(), 1);
}

// AGENT-NOTE: Review finding P1-2 on rejected candidate 3a5ae17: stop retained
// a Ready producer and owner, allowing ReleaseContainer onto the wire. This
// exercises the real producer authority after stop, not a test-only status.
void TaskListOperationAdapterTests::stoppedProducerAdmitsNoOperation() {
  TaskListSource source;
  QVERIFY(source.publishGeneration(
                    {TaskListTest::standalone(QStringLiteral("w1"),
                                              QStringLiteral("app.one"))})
              .ok());
  IdleProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source);
  QVERIFY(producer.start());
  Q_EMIT producerTransport.serviceOwnerChanged(QStringLiteral(":1.1"));
  producer.stop();

  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);
  adapter.releaseContainer(QStringLiteral("c1"), source.revision());
  QCOMPARE(firstResult(finishedSpy).status,
           TaskListOperationStatus::SourceNotReady);
  QCOMPARE(producer.uniqueOwner(), QString());
  QCOMPARE(operations.calls.size(), 0);
}

QTEST_GUILESS_MAIN(TaskListOperationAdapterTests)
#include "tst_task_list_operation_adapter.moc"
