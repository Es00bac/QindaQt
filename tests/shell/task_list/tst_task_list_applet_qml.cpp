// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "../icon_resolution_test_fixture.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

#include "task_list_applet_test_fakes.h"
#include "task_list_applet_qml_theme_fixture.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_TaskListPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskListApplet;
using TaskListAppletTest::FakeTaskListOperationPort;
using TaskListOperationTest::FakeOperationAuthority;

namespace {

QVector<TaskWindowFact> threeEntryFacts() {
  return {TaskListTest::standalone(QStringLiteral("w1"),
                                   QStringLiteral("app.1")),
          TaskListTest::standalone(QStringLiteral("w2"),
                                   QStringLiteral("app.2")),
          TaskListTest::primary(QStringLiteral("w3"),
                                QStringLiteral("app.3"),
                                QStringLiteral("c1")),
          TaskListTest::member(QStringLiteral("w4"), QStringLiteral("c1"))};
}

quint64 publishFacts(TaskListSource &source, FakeOperationAuthority &authority,
                     const QVector<TaskWindowFact> &facts) {
  const auto evaluation = source.publishGeneration(facts);
  if (!evaluation.ok()) {
    return 0;
  }
  authority.revision = source.revision();
  authority.sourceStatus = TaskListSourceStatus::Ready;
  authority.owner = QStringLiteral(":1.1");
  Q_EMIT authority.stateChanged();
  return source.revision();
}

QList<QQuickItem *> visualItemsNamed(QQuickItem *root, const QString &name) {
  QList<QQuickItem *> matches;
  if (root->objectName() == name) {
    matches.append(root);
  }
  for (QQuickItem *child : root->childItems()) {
    matches.append(visualItemsNamed(child, name));
  }
  return matches;
}

QQuickItem *entryButtonFor(QQuickItem *root, const QString &taskId) {
  const auto buttons =
      visualItemsNamed(root, QStringLiteral("taskListEntryButton"));
  for (QQuickItem *button : buttons) {
    if (button->property("entry").toMap().value(QStringLiteral("taskId"))
            .toString()
        == taskId) {
      return button;
    }
  }
  return nullptr;
}

QString focusedTaskIdIn(QQuickItem *root) {
  const auto buttons =
      visualItemsNamed(root, QStringLiteral("taskListEntryButton"));
  for (QQuickItem *button : buttons) {
    if (button->hasActiveFocus()) {
      return button->property("entry")
          .toMap()
          .value(QStringLiteral("taskId"))
          .toString();
    }
  }
  return QString();
}

QString g_appletError;

std::unique_ptr<QObject> createApplet(QQmlEngine &engine,
                                      TaskListAppletController &controller,
                                      bool vertical) {
  QQmlComponent component(&engine);
  component.loadFromModule(QStringLiteral("QindaQt.Shell.TaskList"),
                           QStringLiteral("TaskListApplet"));
  if (!component.isReady()) {
    g_appletError = component.errorString();
    return nullptr;
  }
  std::unique_ptr<QObject> object(component.createWithInitialProperties(
      {{QStringLiteral("access"), QVariant::fromValue(&controller)},
       {QStringLiteral("vertical"), vertical}}));
  if (object == nullptr) {
    g_appletError = component.errorString();
  }
  return object;
}

void clickCenter(QQuickWindow &window, QQuickItem *item) {
  const QPoint point = item
                           ->mapToScene(QPointF(item->width() / 2,
                                                item->height() / 2))
                           .toPoint();
  QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
}

} // namespace

class TaskListAppletQmlTests final : public QObject {
  Q_OBJECT

private slots:
  void phasesRenderWithTruthfulObjectNames();
  void arrowTraversalStopsAtStripEndpoints();
  void keyboardTraversalAndContextMenuDispatch();
};

void TaskListAppletQmlTests::phasesRenderWithTruthfulObjectNames() {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(
      source, authority, port, {true, true, true},
      [](const QString &) { return QStringLiteral("application-x-executable"); });

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString iconError;
  QVERIFY2(QindaQt::Tests::installResolvedIconFixture(
               engine, QStringLiteral(QINDAQT_APPLET_ICON_FIXTURE_ROOT),
               {QStringLiteral("application-x-executable"),
                QStringLiteral("window-restore-symbolic")}, &iconError),
           qPrintable(iconError));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  auto owned = createApplet(engine, controller, false);
  QVERIFY2(owned != nullptr, qPrintable(g_appletError));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  root->setParentItem(window.contentItem());
  window.show();
  QTRY_VERIFY(window.isExposed());

  QAccessibleInterface *rootInterface =
      QAccessible::queryAccessibleInterface(root);
  QVERIFY(rootInterface != nullptr);
  QCOMPARE(rootInterface->role(), QAccessible::Grouping);
  QCOMPARE(rootInterface->text(QAccessible::Name),
           QStringLiteral("Task list"));

  const auto visibleNamed = [&root](const QString &name) {
    auto *item = root->findChild<QQuickItem *>(name);
    return item != nullptr && item->isVisible();
  };

  QCOMPARE(controller.phaseText(), QStringLiteral("loading"));
  QVERIFY(!visibleNamed(QStringLiteral("taskListLoadingLabel"))); QVERIFY(visibleNamed(QStringLiteral("taskListPhaseIcon")));

  QVERIFY(publishFacts(source, authority, {}) > 0);
  QCOMPARE(controller.phaseText(), QStringLiteral("empty"));
  QVERIFY(!visibleNamed(QStringLiteral("taskListEmptyLabel"))); QVERIFY(visibleNamed(QStringLiteral("taskListPhaseIcon")));
  QVERIFY(!visibleNamed(QStringLiteral("taskListLoadingLabel")));

  QVERIFY(publishFacts(source, authority, threeEntryFacts()) > 0);
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
  QCOMPARE(visualItemsNamed(root, QStringLiteral("taskListEntryButton")).size(),
           3);
  QVERIFY(!visibleNamed(QStringLiteral("taskListOverflowIndicator")));
  QVERIFY(!visibleNamed(QStringLiteral("taskListEmptyLabel")));
  QQuickItem *containerButton = entryButtonFor(root, QStringLiteral("c1"));
  QVERIFY(containerButton != nullptr);
  QVERIFY(containerButton->height() <= 28.0);
  auto *entryIcon = containerButton->findChild<QQuickItem *>(
      QStringLiteral("taskListEntryIcon"));
  QVERIFY(entryIcon != nullptr);
  QVERIFY(QindaQt::Tests::hasResolvedProviderSource(
      entryIcon, QStringLiteral("window-restore-symbolic")));
  auto *countBadge =
      containerButton->findChild<QQuickItem *>(
          QStringLiteral("taskListEntryCountBadge"));
  QVERIFY(countBadge != nullptr);
  QVERIFY(countBadge->isVisible());
  QCOMPARE(countBadge->property("text").toString(), QStringLiteral("×2"));
  QAccessibleInterface *rowInterface =
      QAccessible::queryAccessibleInterface(containerButton);
  QVERIFY(rowInterface != nullptr);
  QCOMPARE(rowInterface->role(), QAccessible::Button);
  QCOMPARE(rowInterface->text(QAccessible::Name),
           controller.projection().rows.at(2).accessibleName);

  // Overflow truth: 70 scoped entries render 64 rows plus an exact indicator.
  QVector<TaskWindowFact> bulk;
  bulk.reserve(70);
  for (int index = 0; index < 70; ++index) {
    bulk.append(TaskListTest::standalone(
        QStringLiteral("bulk-%1").arg(index, 3, 10, QLatin1Char('0')),
        QStringLiteral("app.bulk")));
  }
  QVERIFY(publishFacts(source, authority, bulk) > 0);
  QCOMPARE(controller.entryCount(), 64);
  QCOMPARE(controller.totalEntryCount(), 70);
  QCOMPARE(controller.overflowCount(), 6);
  QCOMPARE(visualItemsNamed(root, QStringLiteral("taskListEntryButton")).size(),
           64);
  auto *overflow =
      root->findChild<QQuickItem *>(
          QStringLiteral("taskListOverflowIndicator"));
  QVERIFY(overflow != nullptr);
  QVERIFY(overflow->isVisible());
  QCOMPARE(overflow->property("text").toString(), QStringLiteral("+6 more"));

  // Degraded keeps the retained rows and adds the badge.
  source.markDegraded();
  authority.setUnavailable();
  QCOMPARE(controller.phaseText(), QStringLiteral("degraded"));
  QVERIFY(visibleNamed(QStringLiteral("taskListDegradedBadge")));
  QCOMPARE(visualItemsNamed(root, QStringLiteral("taskListEntryButton")).size(),
           64);

  // A fresh generation returns to ready; the vertical orientation renders the
  // same rows.
  QVERIFY(publishFacts(source, authority, threeEntryFacts()) > 0);
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
  auto verticalOwned = createApplet(engine, controller, true);
  QVERIFY2(verticalOwned != nullptr, qPrintable(g_appletError));
  auto *verticalRoot = qobject_cast<QQuickItem *>(verticalOwned.get());
  QVERIFY(verticalRoot != nullptr);
  verticalRoot->setParentItem(window.contentItem());
  QCOMPARE(
      visualItemsNamed(verticalRoot, QStringLiteral("taskListEntryButton"))
          .size(),
      3);
  verticalRoot->setParentItem(nullptr);
  verticalOwned.reset();

  // Refusal feedback surfaces as a popup and the dismiss button clears it.
  QCOMPARE(controller.activateTask(QStringLiteral("w1"), quint64(999)), false);
  QCOMPARE(controller.feedbackPresent(), true);
  auto *popup =
      root->findChild<QObject *>(QStringLiteral("taskListFeedbackPopup"));
  QVERIFY(popup != nullptr);
  QTRY_VERIFY(popup->property("visible").toBool());
  auto *feedbackText =
      root->findChild<QQuickItem *>(
          QStringLiteral("taskListFeedbackText"));
  QVERIFY(feedbackText != nullptr);
  QCOMPARE(feedbackText->property("text").toString(), controller.feedback());
  auto *dismiss =
      root->findChild<QQuickItem *>(
          QStringLiteral("taskListFeedbackDismiss"));
  QVERIFY(dismiss != nullptr);
  clickCenter(window, dismiss);
  QTRY_VERIFY(!controller.feedbackPresent());
  QTRY_VERIFY(!popup->property("visible").toBool());

  // Read denial renders the unavailable row with the machine reason exposed.
  TaskListSource deniedSource;
  FakeOperationAuthority deniedAuthority;
  FakeTaskListOperationPort deniedPort;
  TaskListAppletController deniedController(deniedSource, deniedAuthority,
                                            deniedPort, {false, true, true});
  auto deniedOwned = createApplet(engine, deniedController, false);
  QVERIFY2(deniedOwned != nullptr, qPrintable(g_appletError));
  auto *deniedRoot = qobject_cast<QQuickItem *>(deniedOwned.get());
  QVERIFY(deniedRoot != nullptr);
  deniedRoot->setParentItem(window.contentItem());
  QCOMPARE(deniedController.phaseText(), QStringLiteral("unavailable"));
  auto *unavailableLabel =
      deniedRoot->findChild<QQuickItem *>(
          QStringLiteral("taskListUnavailableLabel"));
  QVERIFY(unavailableLabel != nullptr);
  QVERIFY(!unavailableLabel->isVisible());
  auto *deniedPhaseIcon = deniedRoot->findChild<QQuickItem *>(QStringLiteral("taskListPhaseIcon")); QVERIFY(deniedPhaseIcon != nullptr); QVERIFY(deniedPhaseIcon->isVisible());
  deniedRoot->setParentItem(nullptr);
}

void TaskListAppletQmlTests::arrowTraversalStopsAtStripEndpoints() {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port,
                                      {true, true, true});
  QVERIFY(publishFacts(source, authority, threeEntryFacts()) > 0);

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  window.show();
  QTRY_VERIFY(window.isExposed());

  // Horizontal strip: Left/Right traverse in canonical order, stop at both
  // endpoints (KeyNavigation is null past the edge), and vertical arrows are
  // inert.
  auto owned = createApplet(engine, controller, false);
  QVERIFY2(owned != nullptr, qPrintable(g_appletError));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  root->setParentItem(window.contentItem());
  QQuickItem *first = entryButtonFor(root, QStringLiteral("w1"));
  QVERIFY(first != nullptr);
  first->forceActiveFocus();
  QVERIFY(first->hasActiveFocus());
  QTest::keyClick(&window, Qt::Key_Right);
  QCOMPARE(focusedTaskIdIn(root), QStringLiteral("w2"));
  QTest::keyClick(&window, Qt::Key_Right);
  QCOMPARE(focusedTaskIdIn(root), QStringLiteral("c1"));
  QTest::keyClick(&window, Qt::Key_Right);
  QCOMPARE(focusedTaskIdIn(root), QStringLiteral("c1"));
  QTest::keyClick(&window, Qt::Key_Down);
  QCOMPARE(focusedTaskIdIn(root), QStringLiteral("c1"));
  QTest::keyClick(&window, Qt::Key_Left);
  QCOMPARE(focusedTaskIdIn(root), QStringLiteral("w2"));
  QTest::keyClick(&window, Qt::Key_Left);
  QCOMPARE(focusedTaskIdIn(root), QStringLiteral("w1"));
  QTest::keyClick(&window, Qt::Key_Left);
  QCOMPARE(focusedTaskIdIn(root), QStringLiteral("w1"));
  root->setParentItem(nullptr);
  owned.reset();

  // Vertical strip: Up/Down traverse with the same endpoint fencing and the
  // horizontal arrows stay inert.
  auto verticalOwned = createApplet(engine, controller, true);
  QVERIFY2(verticalOwned != nullptr, qPrintable(g_appletError));
  auto *verticalRoot = qobject_cast<QQuickItem *>(verticalOwned.get());
  QVERIFY(verticalRoot != nullptr);
  verticalRoot->setParentItem(window.contentItem());
  QQuickItem *verticalFirst =
      entryButtonFor(verticalRoot, QStringLiteral("w1"));
  QVERIFY(verticalFirst != nullptr);
  verticalFirst->forceActiveFocus();
  QVERIFY(verticalFirst->hasActiveFocus());
  QTest::keyClick(&window, Qt::Key_Down);
  QCOMPARE(focusedTaskIdIn(verticalRoot), QStringLiteral("w2"));
  QTest::keyClick(&window, Qt::Key_Down);
  QCOMPARE(focusedTaskIdIn(verticalRoot), QStringLiteral("c1"));
  QTest::keyClick(&window, Qt::Key_Down);
  QCOMPARE(focusedTaskIdIn(verticalRoot), QStringLiteral("c1"));
  QTest::keyClick(&window, Qt::Key_Right);
  QCOMPARE(focusedTaskIdIn(verticalRoot), QStringLiteral("c1"));
  QTest::keyClick(&window, Qt::Key_Up);
  QCOMPARE(focusedTaskIdIn(verticalRoot), QStringLiteral("w2"));
  QTest::keyClick(&window, Qt::Key_Up);
  QCOMPARE(focusedTaskIdIn(verticalRoot), QStringLiteral("w1"));
  QTest::keyClick(&window, Qt::Key_Up);
  QCOMPARE(focusedTaskIdIn(verticalRoot), QStringLiteral("w1"));
  verticalRoot->setParentItem(nullptr);
  verticalOwned.reset();

  // Traversal dispatched no operation.
  QCOMPARE(port.calls.size(), 0);
}

void TaskListAppletQmlTests::keyboardTraversalAndContextMenuDispatch() {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port,
                                      {true, true, true});
  const quint64 revision = publishFacts(source, authority, threeEntryFacts());
  QVERIFY(revision > 0);

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  auto owned = createApplet(engine, controller, false);
  QVERIFY2(owned != nullptr, qPrintable(g_appletError));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  root->setParentItem(window.contentItem());
  window.show();
  QTRY_VERIFY(window.isExposed());

  const auto commitLast = [&port] {
    port.complete(port.lastCall(), TaskListOperationStatus::Committed, {}, {});
  };
  const auto focusedTaskId = [&root]() { return focusedTaskIdIn(root); };

  // Tab/Backtab traverse the entry buttons in canonical order.
  QQuickItem *first = entryButtonFor(root, QStringLiteral("w1"));
  QVERIFY(first != nullptr);
  first->forceActiveFocus();
  QVERIFY(first->hasActiveFocus());
  QTest::keyClick(&window, Qt::Key_Tab);
  QCOMPARE(focusedTaskId(), QStringLiteral("w2"));
  QTest::keyClick(&window, Qt::Key_Tab);
  QCOMPARE(focusedTaskId(), QStringLiteral("c1"));
  QTest::keyClick(&window, Qt::Key_Backtab);
  QCOMPARE(focusedTaskId(), QStringLiteral("w2"));

  // Space activates the focused row with the exact displayed revision.
  QTest::keyClick(&window, Qt::Key_Space);
  QCOMPARE(port.calls.size(), 1);
  QCOMPARE(port.lastCall().method, QStringLiteral("executeTaskIntent"));
  QCOMPARE(port.lastCall().request.taskId, QStringLiteral("w2"));
  QCOMPARE(port.lastCall().request.kind, TaskIntentKind::Activate);
  QCOMPARE(port.lastCall().request.expectedRevision, revision);
  QCOMPARE(port.lastCall().revision, revision);

  // AGENT-GUARD: Reprojection rebuilds delegates; re-fetch rows after it or a
  // held QQuickItem* dangles. The pending row skips further dispatch.
  QQuickItem *pendingRow = entryButtonFor(root, QStringLiteral("w2"));
  QVERIFY(pendingRow != nullptr);
  QVERIFY(!pendingRow->isEnabled());
  QCOMPARE(pendingRow->property("entry")
               .toMap()
               .value(QStringLiteral("pending"))
               .toBool(),
           true);
  QTest::keyClick(&window, Qt::Key_Space);
  QCOMPARE(port.calls.size(), 1);
  commitLast();
  QQuickItem *restored = entryButtonFor(root, QStringLiteral("w2"));
  QVERIFY(restored != nullptr);
  QVERIFY(restored->isEnabled());

  // Return activates too.
  first = entryButtonFor(root, QStringLiteral("w1"));
  QVERIFY(first != nullptr);
  first->forceActiveFocus();
  QVERIFY(first->hasActiveFocus());
  QTest::keyClick(&window, Qt::Key_Return);
  QCOMPARE(port.calls.size(), 2);
  QCOMPARE(port.lastCall().request.taskId, QStringLiteral("w1"));
  QCOMPARE(port.lastCall().request.expectedRevision, revision);
  commitLast();

  const auto openMenuOn = [&root, &window](const QString &taskId) {
    QQuickItem *row = entryButtonFor(root, taskId);
    if (row == nullptr) {
      return static_cast<QObject *>(nullptr);
    }
    row->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Menu);
    auto *menu =
        row->findChild<QObject *>(QStringLiteral("taskListContextMenu"));
    return menu;
  };
  const auto triggerItem = [&window](QObject *menu, const QString &name) {
    auto *item = menu->findChild<QQuickItem *>(name);
    if (item == nullptr || !item->isVisible()) {
      return false;
    }
    clickCenter(window, item);
    return true;
  };

  QObject *menu = openMenuOn(QStringLiteral("c1"));
  QVERIFY(menu != nullptr);
  QTRY_VERIFY(menu->property("visible").toBool());
  QVERIFY(triggerItem(menu, QStringLiteral("taskListContextMinimize")));
  QTRY_COMPARE(port.calls.size(), 3);
  QCOMPARE(port.lastCall().request.kind, TaskIntentKind::Minimize);
  QCOMPARE(port.lastCall().request.taskId, QStringLiteral("c1"));
  QCOMPARE(port.lastCall().request.expectedRevision, revision);
  commitLast();

  menu = openMenuOn(QStringLiteral("c1"));
  QVERIFY(menu != nullptr);
  QTRY_VERIFY(menu->property("visible").toBool());
  QVERIFY(triggerItem(menu, QStringLiteral("taskListContextUngroup")));
  QTRY_COMPARE(port.calls.size(), 4);
  QCOMPARE(port.lastCall().method, QStringLiteral("releaseContainer"));
  QCOMPARE(port.lastCall().firstId, QStringLiteral("c1"));
  QCOMPARE(port.lastCall().revision, revision);
  commitLast();

  menu = openMenuOn(QStringLiteral("w1"));
  QVERIFY(menu != nullptr);
  QTRY_VERIFY(menu->property("visible").toBool());
  auto *ungroupItem =
      menu->findChild<QQuickItem *>(
          QStringLiteral("taskListContextUngroup"));
  QVERIFY(ungroupItem != nullptr);
  QVERIFY(!ungroupItem->isVisible());
  QVERIFY(triggerItem(menu, QStringLiteral("taskListContextClose")));
  QTRY_COMPARE(port.calls.size(), 5);
  QCOMPARE(port.lastCall().request.kind, TaskIntentKind::Close);
  QCOMPARE(port.lastCall().request.taskId, QStringLiteral("w1"));
  QCOMPARE(port.lastCall().request.expectedRevision, revision);
  commitLast();

  menu = openMenuOn(QStringLiteral("w1"));
  QVERIFY(menu != nullptr);
  QTRY_VERIFY(menu->property("visible").toBool());
  QVERIFY(triggerItem(menu, QStringLiteral("taskListContextRaise")));
  QTRY_COMPARE(port.calls.size(), 6);
  QCOMPARE(port.lastCall().request.kind, TaskIntentKind::Raise);
  QCOMPARE(port.lastCall().request.taskId, QStringLiteral("w1"));
  QCOMPARE(port.lastCall().request.expectedRevision, revision);
}

QTEST_MAIN(TaskListAppletQmlTests)
#include "tst_task_list_applet_qml.moc"
