// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include "qindaqt/shell/icons/icon_runtime.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
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
using TaskListAppletTest::FakePreviewPort;
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
    void overflowBeyondTaskbarCapScrollsInDockMode();
    void pointerMagnificationSwellsTilesWithoutMovingLayout();
    void dragStateCommitsReorderThroughTheController();
    void hoverPreviewShowsImageCardOrTitleFallback();
    void groupingReplacesApplicationsWithOneColoredContainer_data();
    void groupingReplacesApplicationsWithOneColoredContainer();
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

void TaskListAppletDockQmlTests::overflowBeyondTaskbarCapScrollsInDockMode()
{
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port, {true, true, true});

  // One window past the taskbar presentation cap: the taskbar strip would
  // truncate with "+N more"; a dock host raises the bound and the zone
  // viewport scrolls instead.
  QVector<TaskWindowFact> facts;
  facts.reserve(size_t(kMaxPresentedTaskEntries) + 1);
  for (int index = 0; index <= kMaxPresentedTaskEntries; ++index) {
    facts.append(TaskListTest::standalone(
        QStringLiteral("w-%1").arg(index, 4, 10, QLatin1Char('0')),
        QStringLiteral("app.%1").arg(index, 4, 10, QLatin1Char('0'))));
  }
  QVERIFY(source.publishGeneration(facts).ok());
  authority.revision = source.revision();
  authority.sourceStatus = TaskListSourceStatus::Ready;
  authority.owner = QStringLiteral(":1.1");
  Q_EMIT authority.stateChanged();

  QCOMPARE(controller.entryCount(), kMaxPresentedTaskEntries);
  QCOMPARE(controller.overflowCount(), 1);
  controller.setPresentationLimit(kMaxPresentedDockEntries);
  QCOMPARE(controller.entryCount(), kMaxPresentedTaskEntries + 1);
  QCOMPARE(controller.overflowCount(), 0);

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

  // Every raised-limit row is projected into the strip: no overflow truth
  // remains, so the "+N more" indicator never appears and the host viewport
  // scrolls instead.
  QCOMPARE(controller.entryRows().size(), kMaxPresentedTaskEntries + 1);
  QCOMPARE(controller.overflowCount(), 0);
  auto *indicator = root->findChild<QQuickItem *>(
      QStringLiteral("taskListOverflowIndicator"));
  QVERIFY(indicator != nullptr && !indicator->isVisible());
  QTRY_VERIFY(dockEntry(root) != nullptr);
}

void TaskListAppletDockQmlTests::pointerMagnificationSwellsTilesWithoutMovingLayout()
{
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port, {true, true, true});

  QVector<TaskWindowFact> facts;
  for (int index = 0; index < 5; ++index) {
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
  QTRY_VERIFY(dockEntry(root) != nullptr);

  // The strip layout is the AGENT-GUARD truth: magnification must never move
  // delegate bounds, so the tiles keep their exact positions under hover.
  const qreal firstRestX = dockEntry(root)->x();
  const qreal tileWidth = dockEntry(root)->width();
  const qreal stripRestWidth = root->implicitWidth();

  const auto tileAt = [&root](int index) {
    // Repeater delegates are visual children rather than QObject children,
    // so enumerate the QQuickItem tree in strip order.
    int seen = 0;
    std::function<QQuickItem *(QQuickItem *)> walk =
        [&](QQuickItem *item) -> QQuickItem * {
      if (item->objectName() == QLatin1StringView("taskListEntryButton")) {
        if (seen++ == index) {
          return item;
        }
      }
      for (QQuickItem *child : item->childItems()) {
        if (QQuickItem *hit = walk(child); hit != nullptr) {
          return hit;
        }
      }
      return nullptr;
    };
    return walk(root);
  };

  // Pointer at the first tile's center (the strip-local x the HoverHandler
  // would report): that tile peaks and its neighbor participates with a
  // smaller falloff. The falloff contract is driven through dockPointerX
  // directly, because synthetic cross-test pointer state makes raw
  // QTest::mouseMove delivery nondeterministic in the offscreen harness.
  QQuickItem *first = tileAt(0);
  QQuickItem *second = tileAt(1);
  QVERIFY(first != nullptr && second != nullptr);
  auto *firstIcon =
      first->findChild<QQuickItem *>(QStringLiteral("taskListDockEntryIcon"));
  auto *secondIcon =
      second->findChild<QQuickItem *>(QStringLiteral("taskListDockEntryIcon"));
  QVERIFY(firstIcon != nullptr && secondIcon != nullptr);

  root->setProperty("dockPointerX", first->x() + first->width() / 2);
  // The magnification factor is the tile's own binding (the icon renders it
  // through its transform); the rendered scale animates, so exactness is
  // asserted on dockZoomScale.
  QTRY_COMPARE(first->property("dockZoomScale").toDouble(), 1.5);
  const qreal neighborScale = second->property("dockZoomScale").toDouble();
  QVERIFY(neighborScale > 1.0);
  QVERIFY(neighborScale < 1.5);
  QCOMPARE(first->width(), tileWidth);
  QCOMPARE(first->x(), firstRestX);
  QCOMPARE(root->implicitWidth(), stripRestWidth);

  // Pointer outside the strip (-1): every tile returns to rest scale.
  root->setProperty("dockPointerX", -1.0);
  QTRY_COMPARE(first->property("dockZoomScale").toDouble(), 1.0);
  QCOMPARE(second->property("dockZoomScale").toDouble(), 1.0);

  // The host quick setting disables the zoom entirely: a pointer value alone
  // never swells a tile, and the strip layout stays untouched.
  root->setProperty("dockZoomEnabled", false);
  root->setProperty("dockPointerX", first->x() + first->width() / 2);
  QTRY_COMPARE(first->property("dockZoomScale").toDouble(), 1.0);
  QCOMPARE(second->property("dockZoomScale").toDouble(), 1.0);
  QCOMPARE(root->implicitWidth(), stripRestWidth);
}

void TaskListAppletDockQmlTests::dragStateCommitsReorderThroughTheController()
{
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port, {true, true, true});
  QVector<TaskWindowFact> facts;
  for (int index = 1; index <= 3; ++index) {
    facts.append(TaskListTest::standalone(
        QStringLiteral("w%1").arg(index),
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
  QString error;
  auto owned = createDockApplet(engine, controller, &error);
  QVERIFY2(owned != nullptr, qPrintable(error));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QTRY_VERIFY(dockEntry(root) != nullptr);

  // Drive the strip's drag state machine directly: pressing tile 2 (w3) and
  // dropping it into the gap before tile 0 commits reorderTask with the
  // displayed revision. Reorder is presentation preference: no compositor
  // operation may dispatch, so the port stays silent.
  const qreal slotExtent = root->property("dockSlotExtent").toReal();
  QVERIFY(slotExtent > 0);
  QVERIFY(QMetaObject::invokeMethod(root, "dockDragBegin", Q_ARG(QVariant, 2)));
  QVERIFY(root->property("dockDragActive").toBool());
  QVERIFY(QMetaObject::invokeMethod(root, "dockDragUpdate",
                                    Q_ARG(QVariant, slotExtent * 1.0)));
  QCOMPARE(root->property("dockDragTo").toInt(), 1);
  QVERIFY(QMetaObject::invokeMethod(root, "dockDragEnd"));
  QVERIFY(!root->property("dockDragActive").toBool());
  QCOMPARE(port.calls.size(), 0);
  // Slot 1 is the gap between w1 and w2, so w3 lands between them.
  QTRY_COMPARE(controller.entryRows().at(0).toMap().value(QStringLiteral("taskId")),
               QVariant(QStringLiteral("w1")));
  QCOMPARE(controller.entryRows().at(1).toMap().value(QStringLiteral("taskId")),
               QVariant(QStringLiteral("w3")));
  QCOMPARE(controller.entryRows().at(2).toMap().value(QStringLiteral("taskId")),
               QVariant(QStringLiteral("w2")));
  QCOMPARE(controller.userTaskOrder(),
           (QStringList{QStringLiteral("w1"), QStringLiteral("w3"),
                        QStringLiteral("w2")}));

  // Dropping a tile back into its own slot commits nothing.
  QVERIFY(QMetaObject::invokeMethod(root, "dockDragBegin", Q_ARG(QVariant, 0)));
  QVERIFY(QMetaObject::invokeMethod(root, "dockDragUpdate", Q_ARG(QVariant, 0.0)));
  QVERIFY(QMetaObject::invokeMethod(root, "dockDragEnd"));
  QCOMPARE(controller.userTaskOrder().size(), 3);
  QCOMPARE(port.calls.size(), 0);
  QVERIFY(!controller.feedbackPresent());
}

void TaskListAppletDockQmlTests::hoverPreviewShowsImageCardOrTitleFallback()
{
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port, {true, true, true});
  FakePreviewPort previewPort;
  controller.setPreviewPort(&previewPort);
  QVector<TaskWindowFact> facts;
  for (int index = 1; index <= 2; ++index) {
    facts.append(TaskListTest::standalone(
        QStringLiteral("w%1").arg(index),
        QStringLiteral("app.%1").arg(index)));
  }
  QVERIFY(source.publishGeneration(facts).ok());
  authority.revision = source.revision();
  authority.sourceStatus = TaskListSourceStatus::Ready;
  authority.owner = QStringLiteral(":1.1");
  Q_EMIT authority.stateChanged();

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  controller.installPreviewProvider(&engine);
  QString tokenError;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
           qPrintable(tokenError));
  QString error;
  auto owned = createDockApplet(engine, controller, &error);
  QVERIFY2(owned != nullptr, qPrintable(error));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QQuickWindow window;
  window.setGeometry(0, 0, 400, 120);
  root->setParentItem(window.contentItem());
  window.show();
  QTRY_VERIFY(window.isExposed());
  QTRY_VERIFY(dockEntry(root) != nullptr);

  // Hovering the first tile requests a preview through the port.
  QVERIFY(QMetaObject::invokeMethod(root, "previewHover",
                                    Q_ARG(QVariant, 0), Q_ARG(QVariant, true)));
  QCOMPARE(previewPort.calls.size(), 1);
  QCOMPARE(previewPort.calls.last().windowId, QStringLiteral("w1"));
  QVERIFY(root->property("previewVisible").toBool());

  // An available capture decorates the card through the provider token.
  previewPort.autoReply = true;
  previewPort.replyImage = QImage(64, 48, QImage::Format_ARGB32);
  QVERIFY(QMetaObject::invokeMethod(root, "requestPreviewNow"));
  QTRY_VERIFY(root->property("previewToken").toInt() > 0);

  // Leaving the tile closes the card and cancels the in-flight request.
  QVERIFY(QMetaObject::invokeMethod(root, "previewHover",
                                    Q_ARG(QVariant, 0),
                                    Q_ARG(QVariant, false)));
  QCOMPARE(root->property("previewVisible").toBool(), false);

  // Hovering again with an unavailable capture keeps the title-only card.
  previewPort.autoReply = false;
  QVERIFY(QMetaObject::invokeMethod(root, "previewHover",
                                    Q_ARG(QVariant, 1), Q_ARG(QVariant, true)));
  // The 500 ms refresh timer may add in-flight refresh requests for the
  // same tile while the test waits; only the newest tile identity matters.
  QVERIFY(previewPort.calls.size() >= 2);
  QCOMPARE(previewPort.calls.last().windowId, QStringLiteral("w2"));
  QVERIFY(root->property("previewVisible").toBool());
  Q_EMIT previewPort.previewFinished(
      TaskListAppletTest::FakePreviewPort::makeResult(
          QStringLiteral("w2"), static_cast<quint64>(source.revision()), false));
  QVERIFY(root->property("previewVisible").toBool());
  QCOMPARE(root->property("previewToken").toInt(), 0);
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

void TaskListAppletDockQmlTests::groupingReplacesApplicationsWithOneColoredContainer_data()
{
  QTest::addColumn<bool>("vertical");
  QTest::addColumn<bool>("dockMode");
  QTest::newRow("horizontal-dock") << false << true;
  QTest::newRow("vertical-dock") << true << true;
  QTest::newRow("horizontal-taskbar") << false << false;
  QTest::newRow("vertical-taskbar") << true << false;
}

void TaskListAppletDockQmlTests::groupingReplacesApplicationsWithOneColoredContainer()
{
  QFETCH(bool, vertical);
  QFETCH(bool, dockMode);
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
  TaskListAppletController controller(source, authority, port, {true, true, true},
      [](const QString &app) { return app + QStringLiteral("-icon"); });
  const auto publish = [&](const QVector<TaskWindowFact> &facts) {
    if (!source.publishGeneration(facts).ok())
      return false;
    authority.revision = source.revision();
    authority.sourceStatus = TaskListSourceStatus::Ready;
    authority.owner = QStringLiteral(":1.1");
    Q_EMIT authority.stateChanged();
    return true;
  };
  const auto firefox = TaskListTest::standalone(QStringLiteral("firefox-window"),
                                               QStringLiteral("firefox"));
  const auto mail = TaskListTest::standalone(QStringLiteral("mail-window"),
                                            QStringLiteral("thunderbird"));
  QVERIFY(publish({firefox, mail}));
  QCOMPARE(controller.entryCount(), 2);
  const quint64 beforeMerge = source.revision();

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
  QVERIFY(QindaQt::Shell::Icons::IconRuntime::install(engine,
      {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")}, {QStringLiteral("QindaQt")}));
  QString error;
  QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &error), qPrintable(error));
  auto owned = createDockApplet(engine, controller, &error);
  QVERIFY2(owned != nullptr, qPrintable(error));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root);
  root->setProperty("vertical", vertical);
  root->setProperty("dockMode", dockMode);
  QQuickWindow window;
  window.setGeometry(0, 0, 900, 220);
  root->setParentItem(window.contentItem());
  window.show();
  QTRY_VERIFY(window.isExposed());

  auto primary = firefox;
  primary.role = TaskWindowRole::ContainerPrimary;
  primary.containerId = QStringLiteral("group");
  primary.colorHex = QStringLiteral("#269CDA");
  auto member = mail;
  member.role = TaskWindowRole::ContainerMember;
  member.containerId = primary.containerId;
  QVERIFY(publish({primary, member}));
  QCOMPARE(controller.entryCount(), 1);
  QCOMPARE(controller.entryRows().first().toMap().value("taskId").toString(),
           QStringLiteral("group"));
  QTRY_VERIFY(dockEntry(root));
  auto *entry = dockEntry(root);
  const auto verifyIcon = [&]() {
    auto *icon = entry->findChild<QQuickItem *>(dockMode
        ? QStringLiteral("taskListDockEntryIcon") : QStringLiteral("taskListEntryIcon"));
    QVERIFY(icon);
    QCOMPARE(icon->property("name").toString(), QStringLiteral("window-restore-symbolic"));
    QVERIFY(icon->property("symbolic").toBool());
    QVERIFY(icon->property("resolved").toBool());
    QCOMPARE(icon->property("color").value<QColor>(), QColor("#269CDA"));
  };
  verifyIcon();
  QTRY_VERIFY([&]() {
    const QImage frame = window.grabWindow();
    for (int y = 0; y < frame.height(); ++y) {
      for (int x = 0; x < frame.width(); ++x) {
        if (frame.pixelColor(x, y) == QColor("#269CDA"))
          return true;
      }
    }
    return false;
  }());
  QVERIFY(!controller.activateTask(firefox.windowId, beforeMerge));
  QVERIFY(!controller.activateTask(member.windowId, source.revision()));
  QVERIFY(port.calls.isEmpty());

  // AGENT-GUARD: Tabs and focused tiles may change the representative; the
  // sole group row, color, and activation target must follow that generation.
  std::swap(primary, member);
  primary.role = TaskWindowRole::ContainerPrimary;
  primary.colorHex = QStringLiteral("#269CDA");
  member.role = TaskWindowRole::ContainerMember;
  member.colorHex.clear();
  QVERIFY(publish({member, primary}));
  QCOMPARE(controller.entryCount(), 1);
  QTRY_VERIFY(dockEntry(root));
  entry = dockEntry(root);
  verifyIcon();
  QVERIFY(controller.activateTask(QStringLiteral("group"), source.revision()));
  QCOMPARE(port.calls.size(), 1);
  QCOMPARE(port.lastCall().outcome.primaryWindowId, mail.windowId);

  QVERIFY(publish({firefox, mail}));
  QCOMPARE(controller.entryCount(), 2);
  for (const auto &row : controller.entryRows()) {
    const auto map = row.toMap();
    QCOMPARE(map.value("kind").toString(), QStringLiteral("window"));
    QVERIFY(map.value("colorHex").toString().isEmpty());
    QCOMPARE(map.value("iconName").toString(),
             map.value("applicationId").toString() + QStringLiteral("-icon"));
  }
}

QTEST_MAIN(TaskListAppletDockQmlTests)
#include "tst_task_list_applet_dock_qml.moc"
