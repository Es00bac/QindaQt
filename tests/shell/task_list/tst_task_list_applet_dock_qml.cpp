// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

#include "task_list_applet_qml_theme_fixture.h"
#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_TaskListPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskListApplet;
using TaskListAppletTest::FakeTaskListOperationPort;
using TaskListOperationTest::FakeOperationAuthority;

namespace {

quint64 publishDockFacts(TaskListSource &source, FakeOperationAuthority &authority)
{
  const auto evaluation = source.publishGeneration(
      {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1"))});
  if (!evaluation.ok()) {
    return 0;
  }
  authority.revision = source.revision();
  authority.sourceStatus = TaskListSourceStatus::Ready;
  authority.owner = QStringLiteral(":1.1");
  Q_EMIT authority.stateChanged();
  return source.revision();
}

QQuickItem *dockEntry(QQuickItem *root)
{
  if (root->objectName() == QLatin1StringView("taskListEntryButton")) {
    return root;
  }
  for (QQuickItem *child : root->childItems()) {
    if (QQuickItem *entry = dockEntry(child); entry != nullptr) {
      return entry;
    }
  }
  return nullptr;
}

std::unique_ptr<QObject> createDockApplet(QQmlEngine &engine,
                                          TaskListAppletController &controller,
                                          QString *error)
{
  QQmlComponent component(&engine);
  component.loadFromModule(QStringLiteral("QindaQt.Shell.TaskList"),
                           QStringLiteral("TaskListApplet"));
  if (!component.isReady()) {
    *error = component.errorString();
    return nullptr;
  }
  std::unique_ptr<QObject> object(component.createWithInitialProperties(
      {{QStringLiteral("access"), QVariant::fromValue(&controller)},
       {QStringLiteral("dockMode"), true},
       {QStringLiteral("dockTileSize"), 999}}));
  if (object == nullptr) {
    *error = component.errorString();
  }
  return object;
}

} // namespace

class TaskListAppletDockQmlTests final : public QObject {
  Q_OBJECT

private slots:
    void dockModeReservesInteractiveTiles();
    void emptyDockDoesNotReserveATile();
};

void TaskListAppletDockQmlTests::dockModeReservesInteractiveTiles()
{
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port, {true, true, true});
  QVERIFY(publishDockFacts(source, authority) > 0);

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  QString error;
  auto owned = createDockApplet(engine, controller, &error);
  QVERIFY2(owned != nullptr, qPrintable(error));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);

  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  root->setParentItem(window.contentItem());
  window.show();
  QTRY_VERIFY(window.isExposed());

  QCOMPARE(root->property("resolvedDockTileSize").toInt(), 64);
  // Repeater delegates are visual children rather than QObject children;
  // inspect the QQuickItem tree, as the other task-list QML rows do.
  QTRY_VERIFY(root->property("stripVisible").toBool());
  QTRY_VERIFY(dockEntry(root) != nullptr);
  QQuickItem *entry = dockEntry(root);
  QCOMPARE(entry->width(), 64.0);
  QCOMPARE(entry->height(), 64.0);
  auto *icon = entry->findChild<QQuickItem *>(QStringLiteral("taskListDockEntryIcon"));
  QVERIFY(icon != nullptr && icon->isVisible());
  QCOMPARE(icon->property("size").toInt(), 40);
  QVERIFY(entry->findChild<QQuickItem *>(QStringLiteral("taskListRunningIndicator")) != nullptr);
  auto *tooltip = entry->findChild<QObject *>(QStringLiteral("taskListEntryTooltip"));
  QVERIFY(tooltip != nullptr);
  QCOMPARE(tooltip->property("text").toString(),
           entry->property("entry").toMap().value(QStringLiteral("accessibleName")).toString());

  auto *separator = root->findChild<QQuickItem *>(QStringLiteral("taskListDockGroupSeparator"));
  QVERIFY(separator != nullptr && !separator->isVisible());
  root->setProperty("dockHasLauncherGroup", true);
  QTRY_VERIFY(separator->isVisible());

  root->setProperty("reducedMotion", true);
  const QPoint pointer = entry->mapToScene(QPointF(entry->width() / 2,
                                                   entry->height() / 2)).toPoint();
  QTest::mouseMove(&window, pointer);
  QTRY_COMPARE(icon->property("scale").toReal(), 1.0);
  QCOMPARE(icon->property("hoverLift").toReal(), 0.0);

  entry->forceActiveFocus();
  QTest::keyClick(&window, Qt::Key_Return);
  QCOMPARE(port.calls.size(), 1);
  QCOMPARE(port.lastCall().request.taskId, QStringLiteral("w1"));
}

void TaskListAppletDockQmlTests::emptyDockDoesNotReserveATile()
{
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port, {true, true, true});
  const auto evaluation = source.publishGeneration({});
  QVERIFY(evaluation.ok());
  authority.revision = source.revision();
  authority.sourceStatus = TaskListSourceStatus::Ready;
  authority.owner = QStringLiteral(":1.1");
  Q_EMIT authority.stateChanged();

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  QString error;
  auto owned = createDockApplet(engine, controller, &error);
  QVERIFY2(owned != nullptr, qPrintable(error));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QVERIFY(root->property("dockEmpty").toBool());
  QVERIFY(!root->property("visible").toBool());
  QCOMPARE(root->implicitWidth(), 0.0);
  QCOMPARE(root->implicitHeight(), 0.0);
}

QTEST_MAIN(TaskListAppletDockQmlTests)
#include "tst_task_list_applet_dock_qml.moc"
