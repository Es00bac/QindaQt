// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0396: per-tile popups exist only while they are wanted. A Popup.Window
// keeps its own window, and with it a GL context and video memory, for as long
// as the popup object exists, even after it closes, so a menu or tooltip kept
// per tile cost a window for every tile ever right-clicked or hovered. These
// rows count QGuiApplication::allWindows() around real opens and closes.
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QPointer>
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

// One strip over three tiles: two standalone windows and one container.
struct Strip {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller{source, authority, port, {true, true, true}};
  QQmlEngine engine;
  std::unique_ptr<QObject> owned;
  std::unique_ptr<QQuickWindow> window;
  QQuickItem *root = nullptr;
  QString error;

  bool create(bool dockMode) {
    const auto evaluation = source.publishGeneration(
        {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1")),
         TaskListTest::standalone(QStringLiteral("w2"), QStringLiteral("app.2")),
         TaskListTest::primary(QStringLiteral("w3"), QStringLiteral("app.3"),
                               QStringLiteral("c1")),
         TaskListTest::member(QStringLiteral("w4"), QStringLiteral("c1"))});
    if (!evaluation.ok()) {
      error = QStringLiteral("publication refused");
      return false;
    }
    authority.revision = source.revision();
    authority.sourceStatus = TaskListSourceStatus::Ready;
    authority.owner = QStringLiteral(":1.1");
    Q_EMIT authority.stateChanged();

    engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
    if (!TaskListAppletQmlTest::publishTokens(engine, &error)) {
      return false;
    }
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.TaskList"),
                             QStringLiteral("TaskListApplet"));
    if (!component.isReady()) {
      error = component.errorString();
      return false;
    }
    owned.reset(component.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue(&controller)},
         {QStringLiteral("dockMode"), dockMode}}));
    root = qobject_cast<QQuickItem *>(owned.get());
    if (root == nullptr) {
      error = component.errorString();
      return false;
    }
    window = std::make_unique<QQuickWindow>();
    window->setGeometry(0, 0, 900, 300);
    root->setParentItem(window->contentItem());
    // Leave room above the strip: a dock tooltip opens above its tile, and one
    // clamped onto the tile would take the pointer's hover away from it.
    root->setPosition(QPointF(20, 160));
    window->show();
    return true;
  }

  // Repeater delegates are visual children, not QObject children.
  QQuickItem *tile(const QString &taskId) const {
    QQuickItem *found = nullptr;
    const auto visit = [&](auto &&self, QQuickItem *item) -> void {
      if (item->objectName() == QLatin1StringView("taskListEntryButton")
          && item->property("entry").toMap().value(QStringLiteral("taskId")).toString()
              == taskId) {
        found = item;
      }
      for (QQuickItem *child : item->childItems()) {
        self(self, child);
      }
    };
    visit(visit, root);
    return found;
  }
};

} // namespace

class TaskListPopupWindowsTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void contextMenuWindowsAreReleasedAfterTheyClose();
  void dockTooltipWindowIsReleasedWhenNoLongerWanted();
};

// Open each tile's menu once with the Menu key and close it: one popup window
// while it is open, and the window count returns to what it was before any
// menu existed, on every tile.
void TaskListPopupWindowsTests::contextMenuWindowsAreReleasedAfterTheyClose() {
  Strip strip;
  QVERIFY2(strip.create(false), qPrintable(strip.error));
  QTRY_VERIFY(strip.window->isExposed());
  QTRY_VERIFY(strip.tile(QStringLiteral("w1")) != nullptr);

  const qsizetype idleWindows = QGuiApplication::allWindows().size();
  for (const QString &taskId :
       {QStringLiteral("w1"), QStringLiteral("w2"), QStringLiteral("c1")}) {
    QQuickItem *tile = strip.tile(taskId);
    QVERIFY(tile != nullptr);
    // An idle tile owns no menu object at all.
    QVERIFY(tile->findChild<QObject *>(QStringLiteral("taskListContextMenu")) == nullptr);
    tile->forceActiveFocus();
    QVERIFY(tile->hasActiveFocus());
    QTest::keyClick(strip.window.get(), Qt::Key_Menu);
    QPointer<QObject> menu =
        tile->findChild<QObject *>(QStringLiteral("taskListContextMenu"));
    QVERIFY(!menu.isNull());
    QTRY_VERIFY(menu->property("visible").toBool());
    QCOMPARE(QGuiApplication::allWindows().size(), idleWindows + 1);

    QVERIFY(QMetaObject::invokeMethod(menu.data(), "close"));
    QTRY_VERIFY(menu.isNull());
    QTRY_COMPARE(QGuiApplication::allWindows().size(), idleWindows);
    // The offscreen platform leaves native focus on the hidden popup window.
    strip.window->requestActivate();
    QTRY_COMPARE(QGuiApplication::focusWindow(), strip.window.get());
  }
}

// The dock tooltip appears while the tile is hovered, in one popup window, and
// is destroyed with it once the pointer leaves. The strip sits low in the window
// (see Strip::create) because the offscreen platform clamps a tooltip that would
// open above the screen onto the tile, where it takes the pointer's hover away
// and the tooltip flickers away on its own.
void TaskListPopupWindowsTests::dockTooltipWindowIsReleasedWhenNoLongerWanted() {
  Strip strip;
  QVERIFY2(strip.create(true), qPrintable(strip.error));
  QTRY_VERIFY(strip.window->isExposed());
  QTRY_VERIFY(strip.tile(QStringLiteral("w2")) != nullptr);
  QQuickItem *tile = strip.tile(QStringLiteral("w2"));
  strip.root->setProperty("reducedMotion", true);
  QTRY_COMPARE(tile->property("revealProgress").toReal(), 1.0);

  // Park the pointer away from every tile, then hover this one.
  QTest::mouseMove(strip.window.get(), QPoint(strip.window->width() - 2, 2));
  // An idle tile owns neither a tooltip nor its window.
  QVERIFY(tile->findChild<QObject *>(QStringLiteral("taskListEntryTooltip")) == nullptr);
  const qsizetype idleWindows = QGuiApplication::allWindows().size();
  QElapsedTimer sinceHover;
  sinceHover.start();
  QTest::mouseMove(strip.window.get(),
                   tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2)).toPoint());
  QObject *created = nullptr;
  QTRY_VERIFY((created = tile->findChild<QObject *>(QStringLiteral("taskListEntryTooltip"))) != nullptr);
  QPointer<QObject> tooltip(created);
  QVERIFY(!tooltip.isNull());
  QCOMPARE(tooltip->property("text").toString(),
           tile->property("entry").toMap().value(QStringLiteral("accessibleName")).toString());
  QTRY_VERIFY(tooltip->property("visible").toBool());
  // The tooltip keeps its hover delay. A popup created already visible opens at
  // once and skips it, which also opens its window under the pointer.
  const int delay = tooltip->property("delay").toInt();
  QVERIFY2(delay >= 50, "a shorter delay would make the check below prove nothing");
  QVERIFY2(sinceHover.elapsed() >= delay - 20,
           qPrintable(QStringLiteral("opened after %1 ms, delay is %2 ms")
                          .arg(sinceHover.elapsed()).arg(delay)));
  QCOMPARE(QGuiApplication::allWindows().size(), idleWindows + 1);

  QTest::mouseMove(strip.window.get(), QPoint(strip.window->width() - 2, 2));
  QTRY_VERIFY(tooltip.isNull());
  QTRY_COMPARE(QGuiApplication::allWindows().size(), idleWindows);
}

QTEST_MAIN(TaskListPopupWindowsTests)
#include "tst_task_list_popup_windows.moc"
