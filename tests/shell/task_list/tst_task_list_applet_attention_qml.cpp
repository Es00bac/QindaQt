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
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskListApplet;
using TaskListAppletTest::FakeTaskListOperationPort;
using TaskListOperationTest::FakeOperationAuthority;

namespace {

QVector<TaskWindowFact> factsWithUrgentWindow(const QString &urgentId)
{
  auto urgent = TaskListTest::standalone(urgentId, QStringLiteral("app.1"));
  urgent.urgent = true;
  auto quiet = TaskListTest::standalone(QStringLiteral("quiet"),
                                        QStringLiteral("app.2"));
  return {urgent, quiet};
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

// AGENT-GUARD: reprojection rebuilds delegates, so callers must re-fetch rows
// after every publishGeneration; a QQuickItem* held across it dangles.
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

std::unique_ptr<QObject> createApplet(QQmlEngine &engine,
                                      TaskListAppletController &controller,
                                      const QVariantMap &extraProperties = {}) {
  QQmlComponent component(&engine);
  component.loadFromModule(QStringLiteral("QindaQt.Shell.TaskList"),
                           QStringLiteral("TaskListApplet"));
  if (!component.isReady()) {
    return nullptr;
  }
  QVariantMap initialProperties{
      {QStringLiteral("access"), QVariant::fromValue(&controller)},
      {QStringLiteral("vertical"), false}};
  initialProperties.insert(extraProperties);
  return std::unique_ptr<QObject>(
      component.createWithInitialProperties(initialProperties));
}

// The attention pulse object animated by TaskListEntryButton while its
// projected entry is urgent (null until the animation exists).
QObject *attentionPulseIn(QQuickItem *button) {
  return button->findChild<QObject *>(
      QStringLiteral("taskListUrgentAttentionPulse"));
}

// Samples a qreal property every step while the event loop pumps, so a
// running animation is observed mid-cycle rather than only at rest extremes.
qreal minimumSampled(QQuickItem *item, const QString &name, int samples,
                     int stepMs) {
  qreal minimum = item->property(name.toUtf8().constData()).toReal();
  for (int index = 0; index < samples; ++index) {
    QTest::qWait(stepMs);
    minimum = std::min(
        minimum, item->property(name.toUtf8().constData()).toReal());
  }
  return minimum;
}

} // namespace

// Demand-attention truth for the integrated Terminal bell path (audible bell
// -> QApplication::alert -> projected `urgent`): while urgent the badge and
// dock icon breathe opacity, the pulse settles to full when urgency clears,
// and the existing reduced-motion host policy keeps the indication static.
class TaskListAppletAttentionQmlTests final : public QObject {
  Q_OBJECT

private slots:
  void panelBadgeBreathesWhileUrgentAndSettlesWhenCleared();
  void dockIconBreathesWhileUrgentAndSettlesWhenCleared();
  void reducedMotionKeepsAttentionStatic();
};

void TaskListAppletAttentionQmlTests::
    panelBadgeBreathesWhileUrgentAndSettlesWhenCleared() {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port,
                                      {true, true, true});
  QVERIFY(publishFacts(source, authority,
                       factsWithUrgentWindow(QStringLiteral("w1")))
          > 0);

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  auto owned = createApplet(engine, controller);
  QVERIFY(owned != nullptr);
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  root->setParentItem(window.contentItem());
  window.show();
  QTRY_VERIFY(window.isExposed());

  QQuickItem *urgent = entryButtonFor(root, QStringLiteral("w1"));
  QVERIFY(urgent != nullptr);
  auto *badge = urgent->findChild<QQuickItem *>(
      QStringLiteral("taskListEntryUrgentBadge"));
  QVERIFY(badge != nullptr);
  QTRY_VERIFY(badge->isVisible());
  QQuickItem *quiet = entryButtonFor(root, QStringLiteral("quiet"));
  QVERIFY(quiet != nullptr);
  QVERIFY(!quiet->findChild<QQuickItem *>(
               QStringLiteral("taskListEntryUrgentBadge"))
               ->isVisible());

  QObject *pulse = attentionPulseIn(urgent);
  QVERIFY(pulse != nullptr);
  QTRY_VERIFY(pulse->property("running").toBool());
  const qreal minimumLevel =
      minimumSampled(urgent, QStringLiteral("urgentAttentionLevel"), 8, 30);
  QVERIFY2(minimumLevel < 0.9,
           qPrintable(QStringLiteral("attention level never breathed below "
                                     "0.9; minimum sampled %1")
                          .arg(minimumLevel)));

  // Clearing urgency settles the pulse: stopped, full opacity, badge hidden.
  auto settled = TaskListTest::standalone(QStringLiteral("w1"),
                                          QStringLiteral("app.1"));
  auto settledQuiet = TaskListTest::standalone(QStringLiteral("quiet"),
                                               QStringLiteral("app.2"));
  QVERIFY(publishFacts(source, authority, {settled, settledQuiet}) > 0);
  QQuickItem *settledButton = nullptr;
  QTRY_VERIFY((settledButton =
                   entryButtonFor(root, QStringLiteral("w1")))
              != nullptr);
  QObject *settledPulse = attentionPulseIn(settledButton);
  QVERIFY(settledPulse != nullptr);
  QTRY_VERIFY(!settledPulse->property("running").toBool());
  QCOMPARE(settledButton->property("urgentAttentionLevel").toReal(), 1.0);
  auto *settledBadge = settledButton->findChild<QQuickItem *>(
      QStringLiteral("taskListEntryUrgentBadge"));
  QVERIFY(settledBadge != nullptr);
  QTRY_VERIFY(!settledBadge->isVisible());
}

void TaskListAppletAttentionQmlTests::
    dockIconBreathesWhileUrgentAndSettlesWhenCleared() {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port,
                                      {true, true, true});
  QVERIFY(publishFacts(source, authority,
                       factsWithUrgentWindow(QStringLiteral("w1")))
          > 0);

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  auto owned = createApplet(engine, controller,
                            {{QStringLiteral("dockMode"), true},
                             {QStringLiteral("dockTileSize"), 999}});
  QVERIFY(owned != nullptr);
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  root->setParentItem(window.contentItem());
  window.show();
  QTRY_VERIFY(window.isExposed());

  QQuickItem *urgent = entryButtonFor(root, QStringLiteral("w1"));
  QVERIFY(urgent != nullptr);
  auto *dockIcon = urgent->findChild<QQuickItem *>(
      QStringLiteral("taskListDockEntryIcon"));
  QVERIFY(dockIcon != nullptr);
  QTRY_VERIFY(dockIcon->isVisible());
  // Dock rows render no panel badge: the dock tile's attention surface is the
  // icon itself, so its opacity must carry the pulse.
  QVERIFY(!urgent->findChild<QQuickItem *>(
               QStringLiteral("taskListEntryUrgentBadge"))
               ->isVisible());

  QObject *pulse = attentionPulseIn(urgent);
  QVERIFY(pulse != nullptr);
  QTRY_VERIFY(pulse->property("running").toBool());
  const qreal minimumOpacity =
      minimumSampled(dockIcon, QStringLiteral("opacity"), 8, 30);
  QVERIFY2(minimumOpacity < 0.9,
           qPrintable(QStringLiteral("dock icon opacity never breathed below "
                                     "0.9; minimum sampled %1")
                          .arg(minimumOpacity)));

  auto settled = TaskListTest::standalone(QStringLiteral("w1"),
                                          QStringLiteral("app.1"));
  auto settledQuiet = TaskListTest::standalone(QStringLiteral("quiet"),
                                               QStringLiteral("app.2"));
  QVERIFY(publishFacts(source, authority, {settled, settledQuiet}) > 0);
  QQuickItem *settledButton = nullptr;
  QTRY_VERIFY((settledButton =
                   entryButtonFor(root, QStringLiteral("w1")))
              != nullptr);
  QObject *settledPulse = attentionPulseIn(settledButton);
  QVERIFY(settledPulse != nullptr);
  QTRY_VERIFY(!settledPulse->property("running").toBool());
  auto *settledIcon = settledButton->findChild<QQuickItem *>(
      QStringLiteral("taskListDockEntryIcon"));
  QVERIFY(settledIcon != nullptr);
  QTRY_COMPARE(settledIcon->property("opacity").toReal(), 1.0);
}

void TaskListAppletAttentionQmlTests::reducedMotionKeepsAttentionStatic() {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port,
                                      {true, true, true});
  QVERIFY(publishFacts(source, authority,
                       factsWithUrgentWindow(QStringLiteral("w1")))
          > 0);

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  window.show();
  QTRY_VERIFY(window.isExposed());

  // Panel row under the host reduced-motion policy: the badge stays visible
  // and fully opaque; the pulse never starts.
  auto panelOwned = createApplet(engine, controller,
                                 {{QStringLiteral("reducedMotion"), true}});
  QVERIFY(panelOwned != nullptr);
  auto *panelRoot = qobject_cast<QQuickItem *>(panelOwned.get());
  QVERIFY(panelRoot != nullptr);
  panelRoot->setParentItem(window.contentItem());
  QQuickItem *panelUrgent = nullptr;
  QTRY_VERIFY((panelUrgent =
                   entryButtonFor(panelRoot, QStringLiteral("w1")))
              != nullptr);
  auto *badge = panelUrgent->findChild<QQuickItem *>(
      QStringLiteral("taskListEntryUrgentBadge"));
  QVERIFY(badge != nullptr);
  QTRY_VERIFY(badge->isVisible());
  QCOMPARE(panelUrgent->property("urgentAttentionLevel").toReal(), 1.0);
  QTest::qWait(120);
  QCOMPARE(badge->property("opacity").toReal(), 1.0);
  QCOMPARE(minimumSampled(badge, QStringLiteral("opacity"), 6, 60), 1.0);
  QObject *panelPulse = attentionPulseIn(panelUrgent);
  QVERIFY(panelPulse != nullptr);
  QVERIFY(!panelPulse->property("running").toBool());
  panelRoot->setParentItem(nullptr);
  panelOwned.reset();

  // The same policy keeps the dock tile static: icon at full opacity, no
  // running pulse, while urgency stays projected.
  auto dockOwned =
      createApplet(engine, controller,
                   {{QStringLiteral("dockMode"), true},
                    {QStringLiteral("dockTileSize"), 999},
                    {QStringLiteral("reducedMotion"), true}});
  QVERIFY(dockOwned != nullptr);
  auto *dockRoot = qobject_cast<QQuickItem *>(dockOwned.get());
  QVERIFY(dockRoot != nullptr);
  dockRoot->setParentItem(window.contentItem());
  QQuickItem *dockUrgent = nullptr;
  QTRY_VERIFY((dockUrgent = entryButtonFor(dockRoot, QStringLiteral("w1")))
              != nullptr);
  auto *dockIcon = dockUrgent->findChild<QQuickItem *>(
      QStringLiteral("taskListDockEntryIcon"));
  QVERIFY(dockIcon != nullptr);
  QTRY_VERIFY(dockIcon->isVisible());
  QCOMPARE(dockUrgent->property("urgentAttentionLevel").toReal(), 1.0);
  QCOMPARE(minimumSampled(dockIcon, QStringLiteral("opacity"), 6, 60), 1.0);
  QObject *dockPulse = attentionPulseIn(dockUrgent);
  QVERIFY(dockPulse != nullptr);
  QVERIFY(!dockPulse->property("running").toBool());
  dockRoot->setParentItem(nullptr);
}

QTEST_MAIN(TaskListAppletAttentionQmlTests)
#include "tst_task_list_applet_attention_qml.moc"
