// SPDX-License-Identifier: GPL-3.0-or-later
// Focused Dock magnification-envelope fixture, split from
// tst_task_list_applet_dock_qml.cpp (source-shape repair for candidate
// c5664c01). It owns exactly the above-shelf growth contract: the strip's
// reserved tracking surface, peak growth above the shelf inside the panel's
// envelope, exact rest-scale settling, and the reduced/disabled paths.
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtMath>
#include <QtTest>

#include <functional>
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

QQuickItem *dockTileWalk(QQuickItem *root, int &remaining)
{
  if (root->objectName() == QLatin1StringView("taskListEntryButton")) {
    if (remaining == 0) {
      return root;
    }
    --remaining;
    return nullptr;
  }
  for (QQuickItem *child : root->childItems()) {
    if (QQuickItem *entry = dockTileWalk(child, remaining); entry != nullptr) {
      return entry;
    }
  }
  return nullptr;
}

QQuickItem *dockTileAt(QQuickItem *root, int index)
{
  int remaining = index;
  return dockTileWalk(root, remaining);
}

} // namespace

class TaskListDockMagnificationEnvelopeTests final : public QObject {
  Q_OBJECT

private slots:
  void magnifiedTileGrowsAboveTheShelfInsideTheReservedEnvelope();
};

void TaskListDockMagnificationEnvelopeTests::
    magnifiedTileGrowsAboveTheShelfInsideTheReservedEnvelope()
{
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port, {true, true, true});

  QVector<TaskWindowFact> facts;
  for (int index = 0; index < 3; ++index) {
    facts.append(TaskListTest::standalone(
        QStringLiteral("w-%1").arg(index),
        QStringLiteral("app.%1").arg(index)));
  }
  QVERIFY(source.publishGeneration(facts).ok());
  authority.revision = source.revision();
  authority.sourceStatus = TaskListSourceStatus::Ready;
  authority.owner = QStringLiteral(":1.1");
  Q_EMIT authority.stateChanged();

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  QQmlComponent component(&engine);
  component.loadFromModule(QStringLiteral("QindaQt.Shell.TaskList"),
                           QStringLiteral("TaskListApplet"));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  std::unique_ptr<QObject> owned(component.createWithInitialProperties(
      {{QStringLiteral("access"), QVariant::fromValue(&controller)},
       {QStringLiteral("dockMode"), true},
       {QStringLiteral("dockTileSize"), 999}}));
  QVERIFY2(owned != nullptr, qPrintable(component.errorString()));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);

  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  root->setParentItem(window.contentItem());
  window.show();
  QTRY_VERIFY(window.isExposed());
  // Repeater delegates are visual children rather than QObject children;
  // enumerate the QQuickItem tree, as the other task-list QML rows do.
  QTRY_VERIFY(dockTileAt(root, 0) != nullptr);

  // The strip must reserve a pointer-tracking surface that reaches exactly
  // as far above the shelf as the panel viewport exposes and the input/blur
  // masks cover (PanelAppletRow::dockOverscanFor over the same tile): the
  // falloff keeps following a pointer that rides the magnified bump instead
  // of collapsing the zoom at the shelf line.
  const qreal tile = root->property("resolvedDockTileSize").toReal();
  const qreal iconExtent = qBound<qreal>(16, tile - 8, 40);
  const qreal envelope = qCeil(iconExtent - tile / 2) + 3;
  QVERIFY(envelope > 0);
  auto *zoomSurface =
      root->findChild<QQuickItem *>(QStringLiteral("taskListDockZoomSurface"));
  QVERIFY(zoomSurface != nullptr);
  QCOMPARE(zoomSurface->y(), -envelope);
  QCOMPARE(zoomSurface->height(), envelope);
  QCOMPARE(zoomSurface->width(), root->width());
  // The tracking surface lives strictly above the tiles, so it never
  // becomes the hover target over a tile's own hit area.
  QQuickItem *first = dockTileAt(root, 0);
  QQuickItem *second = dockTileAt(root, 1);
  QVERIFY(first != nullptr && second != nullptr);
  QVERIFY(zoomSurface->y() + zoomSurface->height() <= first->y() + 0.01);
  auto *firstIcon =
      first->findChild<QQuickItem *>(QStringLiteral("taskListDockEntryIcon"));
  QVERIFY(firstIcon != nullptr);
  const qreal firstRestX = first->x();
  const qreal firstRestY = first->y();
  const qreal tileHeight = first->height();

  // At the falloff peak the icon paints strictly above the strip top
  // (beyond the resting shelf) while staying inside the reserved envelope,
  // and the delegate bounds never move. The falloff binding peaks exactly
  // under the strip-local pointer; the rendered scale is Behavior-driven
  // and the offscreen platform may drop a stray synthetic hover into the
  // envelope band (the same tracking surface under test), so the rendered
  // assertion is bounded, not exact.
  root->setProperty("dockPointerX", first->x() + first->width() / 2);
  QCOMPARE(first->property("dockZoomScale").toDouble(), 1.5);
  QTRY_VERIFY(firstIcon->property("scale").toDouble() > 1.4);
  QVERIFY(firstIcon->property("scale").toDouble() <= 1.5);
  const qreal iconTop = firstIcon->mapToItem(root, QPointF(0, 0)).y();
  QVERIFY(iconTop < 0.0);
  QVERIFY(iconTop >= -envelope);
  QCOMPARE(first->x(), firstRestX);
  QCOMPARE(first->y(), firstRestY);
  QCOMPARE(first->height(), tileHeight);

  // Pointer exit settles every icon to exactly rest scale. Park the
  // synthetic pointer outside the strip first: the offscreen platform parks
  // a cursor inside the envelope band, whose tracking surface (correctly)
  // keeps rewriting dockPointerX until the pointer truly leaves.
  QTest::mouseMove(&window, QPoint(window.width() - 1, window.height() - 1));
  root->setProperty("dockPointerX", -1.0);
  QTRY_COMPARE(firstIcon->property("scale").toDouble(), 1.0);
  QTRY_COMPARE(second->findChild<QQuickItem *>(
                   QStringLiteral("taskListDockEntryIcon"))
                   ->property("scale")
                   .toDouble(),
               1.0);

  // reducedMotion wins immediately: the magnification binding yields rest
  // scale with no peak value, and the rendered scale settles there.
  root->setProperty("reducedMotion", true);
  root->setProperty("dockPointerX", first->x() + first->width() / 2);
  QCOMPARE(first->property("dockZoomScale").toDouble(), 1.0);
  QTRY_COMPARE(firstIcon->property("scale").toDouble(), 1.0);
  root->setProperty("reducedMotion", false);

  // The disabled dockZoom quick setting behaves the same way.
  root->setProperty("dockZoomEnabled", false);
  root->setProperty("dockPointerX", first->x() + first->width() / 2);
  QCOMPARE(first->property("dockZoomScale").toDouble(), 1.0);
  QTRY_COMPARE(firstIcon->property("scale").toDouble(), 1.0);
}

QTEST_MAIN(TaskListDockMagnificationEnvelopeTests)
#include "tst_task_list_dock_magnification_envelope.moc"
