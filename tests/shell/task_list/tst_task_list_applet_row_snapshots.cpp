// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QJSEngine>
#include <QQmlEngine>
#include <QtTest>

#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskListApplet;

namespace {
struct Fixture {
  TaskListSource source;
  TaskListOperationTest::FakeOperationAuthority authority;
  TaskListAppletTest::FakeTaskListOperationPort port;

  bool publish(const QVector<TaskWindowFact> &facts) {
    if (!source.publishGeneration(facts).ok())
      return false;
    authority.revision = source.revision();
    authority.sourceStatus = TaskListSourceStatus::Ready;
    authority.owner = QStringLiteral(":1.1");
    Q_EMIT authority.stateChanged();
    return true;
  }
};
}

class TaskListAppletRowSnapshotTests final : public QObject {
  Q_OBJECT
private slots:
  void sequenceReadsDoNotResolveMetadata_data();
  void sequenceReadsDoNotResolveMetadata();
  void publicationRefreshesBothSnapshotsBeforeNotification();
  void deniedRowsNeverResolveMetadata();
};

void TaskListAppletRowSnapshotTests::sequenceReadsDoNotResolveMetadata_data() {
  QTest::addColumn<int>("count");
  QTest::newRow("one-window") << 1;
  QTest::newRow("busy-desktop") << 12;
  QTest::newRow("long-running-desktop") << 64;
}

void TaskListAppletRowSnapshotTests::sequenceReadsDoNotResolveMetadata() {
  QFETCH(int, count);
  Fixture fixture;
  int iconNames = 0;
  int iconProbes = 0;
  int applicationNames = 0;
  TaskListAppletController controller(
      fixture.source, fixture.authority, fixture.port, {true, true, true},
      [&](const QString &) { ++iconNames; return QStringLiteral("test-icon"); },
      [&](const QString &) { ++iconProbes; return false; },
      [&](const QString &, const QString &) {
        ++applicationNames; return QStringLiteral("Test application");
      });
  controller.setPresentationLimit(count);
  QVector<TaskWindowFact> facts;
  for (int index = 0; index < count; ++index)
    facts.append(TaskListTest::standalone(QStringLiteral("w%1").arg(index),
                                         QStringLiteral("app%1").arg(index)));
  QVERIFY(fixture.publish(facts));
  QCOMPARE(controller.entryRows().size(), count);
  QCOMPARE(controller.windowRows().size(), count);
  const int publishedNames = iconNames;
  const int publishedProbes = iconProbes;
  const int publishedApplications = applicationNames;
  QVERIFY(publishedProbes > 0);

  // Exercise the actual QV4 reference-sequence path from the live profile:
  // each length/index read can invoke the C++ Q_PROPERTY getter again.
  QJSEngine engine;
  QQmlEngine::setObjectOwnership(&controller, QQmlEngine::CppOwnership);
  engine.globalObject().setProperty(QStringLiteral("controller"),
                                    engine.newQObject(&controller));
  const QJSValue result = engine.evaluate(QStringLiteral(R"JS(
    let visited = 0;
    for (let frame = 0; frame < 1000; ++frame) {
      for (let i = 0; i < controller.entryRows.length; ++i) {
        if (controller.entryRows[i].iconName === "test-icon") ++visited;
      }
      for (let i = 0; i < controller.windowRows.length; ++i) {
        if (controller.windowRows[i].applicationName === "Test application") ++visited;
      }
    }
    visited;
  )JS"));
  QVERIFY2(!result.isError(), qPrintable(result.toString()));
  QCOMPARE(result.toInt(), 2000 * count);
  QCOMPARE(controller.entryRows().size(), count);
  QCOMPARE(controller.windowRows().size(), count);
  QCOMPARE(iconNames, publishedNames);
  QCOMPARE(iconProbes, publishedProbes);
  QCOMPARE(applicationNames, publishedApplications);
}

void TaskListAppletRowSnapshotTests::publicationRefreshesBothSnapshotsBeforeNotification() {
  Fixture fixture;
  QString icon = QStringLiteral("missing-icon");
  QString name = QStringLiteral("Before");
  bool resolved = false;
  TaskListAppletController controller(
      fixture.source, fixture.authority, fixture.port, {true, true, true},
      [&](const QString &) { return icon; },
      [&](const QString &) { return resolved; },
      [&](const QString &, const QString &) { return name; });
  auto fact = TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.one"));
  QVERIFY(fixture.publish({fact}));
  const QVariantList previous = controller.entryRows();
  QCOMPARE(previous.first().toMap().value(QStringLiteral("iconResolved")).toBool(), false);

  icon = QStringLiteral("installed-icon");
  name = QStringLiteral("After");
  resolved = true;
  fact.title = QStringLiteral("Updated title");
  int notifications = 0;
  connect(&controller, &TaskListAppletController::stateReprojected, this, [&] {
    ++notifications;
    for (const QVariantList &rows : {controller.entryRows(), controller.windowRows()}) {
      QCOMPARE(rows.size(), 1);
      const QVariantMap row = rows.first().toMap();
      QCOMPARE(row.value(QStringLiteral("iconName")).toString(), icon);
      QCOMPARE(row.value(QStringLiteral("iconResolved")).toBool(), resolved);
      QCOMPARE(row.value(QStringLiteral("applicationName")).toString(), name);
      QCOMPARE(row.value(QStringLiteral("title")).toString(), fact.title);
      QCOMPARE(row.value(QStringLiteral("generationRevision")).toULongLong(),
               fixture.source.revision());
    }
  });
  QVERIFY(fixture.publish({fact}));
  QCOMPARE(notifications, 1);
  QCOMPARE(previous.first().toMap().value(QStringLiteral("applicationName")).toString(),
           QStringLiteral("Before"));
}

void TaskListAppletRowSnapshotTests::deniedRowsNeverResolveMetadata() {
  Fixture fixture;
  int resolutions = 0;
  TaskListAppletController controller(
      fixture.source, fixture.authority, fixture.port, {false, true, true},
      [&](const QString &) { ++resolutions; return QStringLiteral("test-icon"); },
      [&](const QString &) { ++resolutions; return true; });
  QVERIFY(fixture.publish({TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.one"))}));
  QVERIFY(controller.entryRows().isEmpty());
  QVERIFY(controller.windowRows().isEmpty());
  QCOMPARE(resolutions, 0);
}

QTEST_GUILESS_MAIN(TaskListAppletRowSnapshotTests)
#include "tst_task_list_applet_row_snapshots.moc"
