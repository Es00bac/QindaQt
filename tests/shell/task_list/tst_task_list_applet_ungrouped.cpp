// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QtTest>

#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskListApplet;
using TaskListAppletTest::FakeTaskListOperationPort;
using TaskListOperationTest::FakeOperationAuthority;

namespace {

// Real T0 source, authority fake, and recording port, published through the
// producer boundary: w1 standalone; c1 = primary w2 plus member w3.
struct UngroupedFixture {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;

  quint64 publishReady() {
    const auto evaluation = source.publishGeneration(
        {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.one")),
         TaskListTest::primary(QStringLiteral("w2"), QStringLiteral("app.two"),
                               QStringLiteral("c1")),
         TaskListTest::member(QStringLiteral("w3"), QStringLiteral("c1"))});
    if (!evaluation.ok()) {
      return 0;
    }
    authority.revision = source.revision();
    authority.sourceStatus = TaskListSourceStatus::Ready;
    authority.owner = QStringLiteral(":1.1");
    Q_EMIT authority.stateChanged();
    return source.revision();
  }
};

QString memberIcon(const QString &applicationId) {
  return applicationId == QStringLiteral("app.member")
      ? QStringLiteral("member-icon") : QString();
}

QVariantMap rowForWindow(const QVariantList &rows, const QString &windowId) {
  for (const QVariant &value : rows) {
    const QVariantMap row = value.toMap();
    if (row.value(QStringLiteral("windowId")).toString() == windowId) {
      return row;
    }
  }
  return {};
}

} // namespace

// Ungrouped task-list rows (`grouping: "never"`, ADR-0124 "Luna taskbar
// rendering"): the controller's windowRows view, and the window-targeted
// intents a member row dispatches through T0 arbitration and the container's
// single pending fence.
class TaskListAppletUngroupedTests final : public QObject {
  Q_OBJECT

private slots:
  void windowRowsExpandContainerMembers();
  void windowIntentsTargetTheNamedMember();
};

void TaskListAppletUngroupedTests::windowRowsExpandContainerMembers() {
  UngroupedFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, {true, true, true},
                                      memberIcon);
  QVERIFY(fixture.publishReady() > 0);

  const QVariantList grouped = controller.entryRows();
  QCOMPARE(grouped.size(), 2);
  for (const QVariant &value : grouped) {
    QCOMPARE(value.toMap().value(QStringLiteral("windowId")).toString(),
             QString());
  }

  const QVariantList windows = controller.windowRows();
  QCOMPARE(windows.size(), 3);
  QCOMPARE(controller.windowOverflowCount(), 0);
  const QVariantMap member = rowForWindow(windows, QStringLiteral("w3"));
  QCOMPARE(member.value(QStringLiteral("taskId")).toString(),
           QStringLiteral("c1"));
  QCOMPARE(member.value(QStringLiteral("kind")).toString(),
           QStringLiteral("window"));
  QCOMPARE(member.value(QStringLiteral("title")).toString(),
           QStringLiteral("Title w3"));
  QCOMPARE(member.value(QStringLiteral("iconName")).toString(),
           QStringLiteral("member-icon"));
  QCOMPARE(member.value(QStringLiteral("windowCount")).toUInt(), 1u);
  QCOMPARE(rowForWindow(windows, QStringLiteral("w2"))
               .value(QStringLiteral("taskId"))
               .toString(),
           QStringLiteral("c1"));
}

void TaskListAppletUngroupedTests::windowIntentsTargetTheNamedMember() {
  UngroupedFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, {true, true, true});
  const quint64 revision = fixture.publishReady();
  QVERIFY(revision > 0);

  // Missing or foreign windows are refused before any dispatch.
  QCOMPARE(controller.activateTaskWindow(QStringLiteral("c1"), QString(),
                                         revision),
           false);
  QCOMPARE(controller.activateTaskWindow(QStringLiteral("c1"),
                                         QStringLiteral("w1"), revision),
           false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("the window is no longer listed")));
  QCOMPARE(fixture.port.calls.size(), 0);

  QCOMPARE(controller.activateTaskWindow(QStringLiteral("c1"),
                                         QStringLiteral("w3"), revision),
           true);
  QCOMPARE(fixture.port.calls.size(), 1);
  QCOMPARE(fixture.port.lastCall().method,
           QStringLiteral("executeTaskIntent"));
  QCOMPARE(fixture.port.lastCall().request.taskId, QStringLiteral("c1"));
  QCOMPARE(fixture.port.lastCall().request.windowId, QStringLiteral("w3"));
  QCOMPARE(fixture.port.lastCall().request.kind, TaskIntentKind::Activate);
  QCOMPARE(fixture.port.lastCall().outcome.primaryWindowId,
           QStringLiteral("w3"));
  fixture.port.complete(fixture.port.lastCall(),
                        TaskListOperationStatus::Committed, {}, {});

  QCOMPARE(controller.closeTaskWindow(QStringLiteral("c1"),
                                      QStringLiteral("w3"), revision),
           true);
  QCOMPARE(fixture.port.lastCall().request.kind, TaskIntentKind::Close);
  QCOMPARE(fixture.port.lastCall().outcome.memberWindowIds,
           QStringList{QStringLiteral("w3")});

  // One pending marker fences every row that addresses the container.
  QCOMPARE(controller.activateTaskWindow(QStringLiteral("c1"),
                                         QStringLiteral("w2"), revision),
           false);
  QCOMPARE(fixture.port.calls.size(), 2);
}

QTEST_GUILESS_MAIN(TaskListAppletUngroupedTests)
#include "tst_task_list_applet_ungrouped.moc"
