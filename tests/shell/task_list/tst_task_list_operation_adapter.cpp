// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"

#include <QSignalSpy>
#include <QtTest>

#include "task_list_operation_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace TaskListOperationTest;

// Admission fencing and the Unavailable intent surface: every case here must
// finish without a single byte reaching the transport.
class TaskListOperationAdapterTests final : public QObject {
  Q_OBJECT

private slots:
  void windowIntentsAreUnavailableWithExtensionCodes();
  void containerIntentsAreUnavailableWithExtensionCodes();
  void staleGenerationRejectsBeforeBusTraffic();
  void degradedSourceRejectsBeforeBusTraffic();
  void hybridAuthorityRejectsSubmitAndRelease();
  void unknownContainerIsRejected();
  void busyAdapterRejectsSecondOperation();
  void stoppedProducerAdmitsNoOperation();
};

void TaskListOperationAdapterTests::windowIntentsAreUnavailableWithExtensionCodes() {
  TaskListSource source;
  FakeProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source, fastTiming());
  makeReady(producer, producerTransport);
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);

  const auto activate = acceptIntent(source, QStringLiteral("w1"),
                                     TaskIntentKind::Activate);
  const quint64 token = adapter.executeTaskIntent(
      {QStringLiteral("w1"), TaskIntentKind::Activate, source.revision()},
      activate);
  QCOMPARE(finishedSpy.size(), 1);
  const auto result =
      finishedSpy.constFirst().constFirst().value<TaskListOperationResult>();
  QCOMPARE(result.token, token);
  QCOMPARE(result.status, TaskListOperationStatus::Unavailable);
  QCOMPARE(result.code,
           QStringLiteral("compositor-window-activate-unavailable"));
  QCOMPARE(operations.calls.size(), 0);

  const auto minimize = acceptIntent(source, QStringLiteral("w1"),
                                     TaskIntentKind::Minimize);
  adapter.executeTaskIntent(
      {QStringLiteral("w1"), TaskIntentKind::Minimize, source.revision()},
      minimize);
  QCOMPARE(finishedSpy.size(), 2);
  QCOMPARE(finishedSpy.at(1).constFirst().value<TaskListOperationResult>().code,
           QStringLiteral("compositor-window-minimize-unavailable"));

  const auto close = acceptIntent(source, QStringLiteral("w1"),
                                  TaskIntentKind::Close);
  adapter.executeTaskIntent(
      {QStringLiteral("w1"), TaskIntentKind::Close, source.revision()}, close);
  QCOMPARE(finishedSpy.size(), 3);
  QCOMPARE(finishedSpy.at(2).constFirst().value<TaskListOperationResult>().code,
           QStringLiteral("compositor-window-close-unavailable"));
}

void TaskListOperationAdapterTests::containerIntentsAreUnavailableWithExtensionCodes() {
  TaskListSource source;
  FakeProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source, fastTiming());
  makeReady(producer, producerTransport);
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);

  const auto outcome = acceptIntent(source, QStringLiteral("c1"),
                                    TaskIntentKind::Activate);
  QCOMPARE(outcome.entryKind, TaskEntryKind::Container);
  adapter.executeTaskIntent(
      {QStringLiteral("c1"), TaskIntentKind::Activate, source.revision()},
      outcome);
  QCOMPARE(finishedSpy.size(), 1);
  QCOMPARE(finishedSpy.constFirst().constFirst()
               .value<TaskListOperationResult>()
               .code,
           QStringLiteral("compositor-container-activate-unavailable"));
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::staleGenerationRejectsBeforeBusTraffic() {
  TaskListSource source;
  FakeProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source, fastTiming());
  makeReady(producer, producerTransport);
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);

  const quint64 token = adapter.releaseContainer(QStringLiteral("c1"),
                                                 quint64(999));
  QCOMPARE(finishedSpy.size(), 1);
  const auto result =
      finishedSpy.constFirst().constFirst().value<TaskListOperationResult>();
  QCOMPARE(result.token, token);
  QCOMPARE(result.status, TaskListOperationStatus::StaleGeneration);
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::degradedSourceRejectsBeforeBusTraffic() {
  TaskListSource source;
  FakeProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source, fastTiming());
  makeReady(producer, producerTransport);
  Q_EMIT producerTransport.serviceOwnerChanged({});
  QCOMPARE(producer.status(), TaskListSourceStatus::Degraded);

  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);
  adapter.releaseContainer(QStringLiteral("c1"), source.revision());
  QCOMPARE(finishedSpy.size(), 1);
  QCOMPARE(finishedSpy.constFirst().constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::SourceNotReady);
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::hybridAuthorityRejectsSubmitAndRelease() {
  TaskListSource source;
  FakeProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source, fastTiming());
  makeReady(producer, producerTransport);
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);

  adapter.activateContainerPage(QStringLiteral("c1"), QStringLiteral("page-2"),
                                source.revision());
  adapter.releaseContainer(QStringLiteral("c1"), source.revision());
  adapter.detachWindow(QStringLiteral("c1"), QStringLiteral("w3"),
                       source.revision());
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
  TaskListSource source;
  FakeProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source, fastTiming());
  makeReady(producer, producerTransport);
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);

  adapter.releaseContainer(QStringLiteral("c-unknown"), source.revision());
  QCOMPARE(finishedSpy.size(), 1);
  QCOMPARE(finishedSpy.constFirst().constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::UnknownContainer);
  QCOMPARE(operations.calls.size(), 0);
}

void TaskListOperationAdapterTests::busyAdapterRejectsSecondOperation() {
  TaskListSource source;
  FakeProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source, fastTiming());
  makeReady(producer, producerTransport);
  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);

  adapter.dockWindows(QStringLiteral("w1"), QStringLiteral("w9"),
                      QStringLiteral("horizontal"), QStringLiteral("second"),
                      0.5, source.revision());
  QVERIFY(adapter.operationInFlight());
  QCOMPARE(operations.calls.size(), 1);

  const quint64 second = adapter.dockWindows(QStringLiteral("w1"),
                                             QStringLiteral("w8"),
                                             QStringLiteral("vertical"),
                                             QStringLiteral("first"), 0.5,
                                             source.revision());
  QCOMPARE(finishedSpy.size(), 1);
  const auto result =
      finishedSpy.constFirst().constFirst().value<TaskListOperationResult>();
  QCOMPARE(result.token, second);
  QCOMPARE(result.status, TaskListOperationStatus::Busy);
  QCOMPARE(operations.calls.size(), 1);
}

// AGENT-NOTE: Review finding P1-2 (rejected candidate 3a5ae17): a stopped
// producer kept its Ready status and owner, so releaseContainer() still
// reached the transport. Stop withdraws availability; admission is fenced
// before any bus traffic.
void TaskListOperationAdapterTests::stoppedProducerAdmitsNoOperation() {
  TaskListSource source;
  FakeProducerTransport producerTransport;
  TaskListFactsProducer producer(producerTransport, source, fastTiming());
  makeReady(producer, producerTransport);
  QCOMPARE(producer.status(), TaskListSourceStatus::Ready);

  FakeOperationTransport operations;
  TaskListOperationAdapter adapter(producer, operations, 100);
  QSignalSpy finishedSpy(&adapter,
                         &TaskListOperationAdapter::operationFinished);

  producer.stop();
  QCOMPARE(producer.status(), TaskListSourceStatus::Degraded);
  adapter.releaseContainer(QStringLiteral("c1"), source.revision());
  QCOMPARE(finishedSpy.size(), 1);
  QCOMPARE(finishedSpy.constFirst()
               .constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::SourceNotReady);
  QCOMPARE(operations.calls.size(), 0);
}

QTEST_GUILESS_MAIN(TaskListOperationAdapterTests)
#include "tst_task_list_operation_adapter.moc"
