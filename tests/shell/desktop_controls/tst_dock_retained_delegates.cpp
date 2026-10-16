// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_controls_qml_test_support.h"
#include "desktop_controls_test_support.h"
#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"

#include <QPointer>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;
using Services::DockItems::DockItem;
using Services::DockItems::DockItems;

namespace {
QHash<QString, QQuickItem *> tiles(QQuickItem *root, const QString &name,
                                   const char *rowProperty, const QString &key) {
  QHash<QString, QQuickItem *> result;
  const auto visit = [&](auto &&self, QQuickItem *item) -> void {
    if (item->objectName() == name)
      result.insert(item->property(rowProperty).toMap().value(key).toString(), item);
    for (QQuickItem *child : item->childItems())
      self(self, child);
  };
  visit(visit, root);
  return result;
}
}

class DockRetainedDelegateTests final : public QObject {
  Q_OBJECT
private slots:
  void quickLaunchRetainsTilesAcrossRunInsertRemoveAndReorder();
  void taskStripRetainsTilesAndAnimationState();
  void quickLaunchTooltipWindowIsReleasedAfterHover();
};

void DockRetainedDelegateTests::quickLaunchRetainsTilesAcrossRunInsertRemoveAndReorder() {
  LauncherStack launcher;
  Shell::Launcher::LauncherAppletController controller(
      &launcher.scanner, &launcher.persistence, &launcher.executor, true);
  QVERIFY(launcher.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Editor"),
                            QStringLiteral("/bin/true")));
  QVERIFY(launcher.addEntry(QStringLiteral("terminal.desktop"), QStringLiteral("Terminal"),
                            QStringLiteral("/bin/true")));
  QVERIFY(launcher.scanner.start());
  launcher.publishDock(*DockItems::fromItems({DockItem::application(QStringLiteral("editor")),
                                             DockItem::trash()}));
  QuickLaunchController quick(&controller, true);
  TaskListStack tasks;
  ShellTaskListApplet::TaskListAppletController taskList(tasks.source, tasks.authority,
                                                         tasks.port, {true, true, true});
  quick.setWindowSource(&taskList, [](const QString &) { return QStringLiteral("editor"); },
                        {true, true});
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &quick, &error, false,
                       true, 60, 60), qPrintable(error));
  const auto current = [&] {
    return tiles(host.item, QStringLiteral("quickLaunchEntry"), "row",
                 QStringLiteral("presentationId"));
  };
  QTRY_COMPARE(current().size(), 2);
  const auto original = current();
  QPointer<QQuickItem> editor = original.value(QStringLiteral("application:editor"));
  QPointer<QQuickItem> trash = original.value(QStringLiteral("trash:"));
  QVERIFY(editor && trash);
  QTRY_COMPARE(editor->property("revealProgress").toReal(), 1.0);
  editor->forceActiveFocus();
  QVERIFY(tasks.publish(TaskListStack::activeEditorFacts()) > 0);
  QTRY_VERIFY(editor->property("running").toBool());
  QCOMPARE(current().value(QStringLiteral("application:editor")), editor.data());
  QCOMPARE(editor->property("revealProgress").toReal(), 1.0);
  QVERIFY(editor->hasActiveFocus());

  QVERIFY(quick.moveItem(0, 2));
  QVERIFY2(editor, "delegate destroyed while admitting move");
  launcher.settleDock();
  QVERIFY2(editor, "delegate destroyed while confirming move");
  QVERIFY(quick.insertApplication(1, QStringLiteral("terminal")));
  QVERIFY2(editor, "delegate destroyed while admitting insertion");
  launcher.settleDock();
  QVERIFY2(editor, "delegate destroyed while confirming insertion");
  QTRY_COMPARE(current().size(), 3);
  QCOMPARE(current().value(QStringLiteral("application:editor")), editor.data());
  QCOMPARE(current().value(QStringLiteral("trash:")), trash.data());
  QCOMPARE(editor->property("visualIndex").toInt(), 2);
  QCOMPARE(editor->property("revealProgress").toReal(), 1.0);
  host.root->setProperty("reducedMotion", true);
  QTRY_COMPARE(current().value(QStringLiteral("application:terminal"))->property("revealProgress").toReal(), 1.0);
  QVERIFY(quick.removeItem(1));
  launcher.settleDock();
  QVERIFY(quick.removeItem(0));
  launcher.settleDock();
  QTRY_COMPARE(current().size(), 1);
  QCOMPARE(current().value(QStringLiteral("application:editor")), editor.data());
  QVERIFY(!current().contains(QStringLiteral("trash:")));
}

void DockRetainedDelegateTests::taskStripRetainsTilesAndAnimationState() {
  TaskListStack tasks;
  ShellTaskListApplet::TaskListAppletController controller(tasks.source, tasks.authority,
                                                            tasks.port, {true, true, true});
  TaskListAppletTest::FakePreviewPort previewPort;
  controller.setPreviewPort(&previewPort);
  auto facts = TaskListStack::activeEditorFacts();
  QVERIFY(tasks.publish(facts) > 0);
  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_DESKTOP_CONTROLS_QML_IMPORT_PATH));
  QVERIFY(publishTokens(engine));
  QQmlComponent component(&engine);
  component.loadFromModule(QStringLiteral("QindaQt.Shell.TaskList"), QStringLiteral("TaskListApplet"));
  std::unique_ptr<QObject> owned(component.createWithInitialProperties(
      {{QStringLiteral("access"), QVariant::fromValue(&controller)},
       {QStringLiteral("dockMode"), true}}));
  QVERIFY2(owned != nullptr, qPrintable(component.errorString()));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QQuickWindow window;
  root->setParentItem(window.contentItem());
  window.show();
  const auto current = [&] {
    return tiles(root, QStringLiteral("taskListEntryButton"), "entry", QStringLiteral("taskId"));
  };
  QTRY_VERIFY(!current().isEmpty());
  const auto original = current();
  const QString id = QStringLiteral("w-editor");
  QQuickItem *tile = original.value(id);
  QVERIFY(tile != nullptr);
  QTRY_COMPARE(tile->property("revealProgress").toReal(), 1.0);
  for (int revision = 0; revision < 100; ++revision) {
    facts[0].title = QStringLiteral("Title %1").arg(revision);
    QVERIFY(tasks.publish(facts) > 0);
    QCOMPARE(current().value(id), tile);
    QCOMPARE(tile->property("revealProgress").toReal(), 1.0);
  }
  // AGENT-CONTRACT (ADR-0390): a retained row carries no snapshot revision, so
  // 100 publications cannot have left it stale. The strip fences a gesture
  // with the controller's revision, which is the generation currently shown.
  QVERIFY(!tile->property("entry").toMap().contains(QStringLiteral("generationRevision")));
  QCOMPARE(controller.generationRevision(), tasks.source.revision());
  const qsizetype callsBefore = tasks.port.calls.size();
  QVERIFY(QMetaObject::invokeMethod(tile, "activate"));
  QCOMPARE(tasks.port.calls.size(), callsBefore + 1);
  QCOMPARE(tasks.port.lastCall().revision, tasks.source.revision());
  tasks.port.complete(tasks.port.lastCall(),
                      ShellTaskList::Operations::TaskListOperationStatus::Committed,
                      QStringLiteral("ok"), {});
  QVERIFY(QMetaObject::invokeMethod(root, "previewHover",
      Q_ARG(QVariant, tile->property("index")), Q_ARG(QVariant, true)));
  QCOMPARE(root->property("previewTaskId").toString(), id);
  QVERIFY(controller.reorderTask(QStringLiteral("w-terminal"), id, tasks.source.revision()));
  QCOMPARE(current().value(id), tile);
  QCOMPARE(root->property("previewIndex").toInt(), tile->property("index").toInt());
  QCOMPARE(root->property("previewTaskId").toString(), id);
  facts.append(TaskListTest::standalone(QStringLiteral("w-new"), QStringLiteral("new.app")));
  QVERIFY(tasks.publish(facts) > 0);
  QCOMPARE(current().size(), 3);
  QCOMPARE(current().value(id), tile);
  facts.removeAt(1);
  QVERIFY(tasks.publish(facts) > 0);
  QCOMPARE(current().size(), 2);
  QCOMPARE(current().value(id), tile);
  QCOMPARE(tile->property("revealProgress").toReal(), 1.0);
  facts.removeAt(0);
  QVERIFY(tasks.publish(facts) > 0);
  QVERIFY(!current().contains(id));
  QCOMPARE(root->property("previewIndex").toInt(), -1);
}

// ADR-0396: a Popup.Window keeps its window, and with it a GL context and video
// memory, for as long as the popup object exists. A tooltip per tile used to
// cost a window for every tile ever hovered; now the tooltip exists only while
// it is wanted and takes its window with it.
void DockRetainedDelegateTests::quickLaunchTooltipWindowIsReleasedAfterHover() {
  LauncherStack launcher;
  Shell::Launcher::LauncherAppletController controller(
      &launcher.scanner, &launcher.persistence, &launcher.executor, true);
  QVERIFY(launcher.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Editor"),
                            QStringLiteral("/bin/true")));
  QVERIFY(launcher.scanner.start());
  launcher.publishDock(*DockItems::fromItems({DockItem::application(QStringLiteral("editor")),
                                             DockItem::trash()}));
  QuickLaunchController quick(&controller, true);
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &quick, &error, false,
                       true, 60, 60), qPrintable(error));
  const auto current = [&] {
    return tiles(host.item, QStringLiteral("quickLaunchEntry"), "row",
                 QStringLiteral("presentationId"));
  };
  QTRY_COMPARE(current().size(), 2);
  QQuickItem *editor = current().value(QStringLiteral("application:editor"));
  QVERIFY(editor != nullptr);
  QTRY_COMPARE(editor->property("revealProgress").toReal(), 1.0);
  // An idle tile owns neither a tooltip nor its window.
  QVERIFY(editor->findChild<QObject *>(QStringLiteral("quickLaunchEntryTooltip")) == nullptr);
  const qsizetype idleWindows = QGuiApplication::allWindows().size();

  const QPoint pointer = editor->mapToScene(QPointF(editor->width() / 2,
                                                    editor->height() / 2)).toPoint();
  QTest::mouseMove(host.window.get(), pointer);
  QTRY_VERIFY(editor->findChild<QObject *>(QStringLiteral("quickLaunchEntryTooltip")) != nullptr);
  QPointer<QObject> tooltip =
      editor->findChild<QObject *>(QStringLiteral("quickLaunchEntryTooltip"));
  QCOMPARE(tooltip->property("text").toString(), editor->property("label").toString());
  QTRY_VERIFY(tooltip->property("visible").toBool());
  QCOMPARE(QGuiApplication::allWindows().size(), idleWindows + 1);

  QTest::mouseMove(host.window.get(),
                   QPoint(host.window->width() - 2, host.window->height() - 2));
  QTRY_VERIFY(tooltip.isNull());
  QTRY_COMPARE(QGuiApplication::allWindows().size(), idleWindows);
}

QTEST_MAIN(DockRetainedDelegateTests)
#include "tst_dock_retained_delegates.moc"
