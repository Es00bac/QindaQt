// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0265: the compiled dock (QuickLaunchApplet) offscreen — tile kinds and
// accessible truth, the group/folder stack, drag to move/group/remove, drops
// with a live gap, and the keyboard menu that repeats every gesture. Gesture
// state machines are driven directly (task-list dock precedent): synthetic
// pointer delivery is nondeterministic in the offscreen harness.
#include "desktop_controls_qml_test_support.h"
#include "desktop_controls_test_support.h"

#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"

#include <QAccessible>
#include <QPointF>
#include <QUrl>
#include <QtTest>

#include <algorithm>

using namespace QindaQt;
using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;
using QindaQt::Services::DockItems::DockItem;
using QindaQt::Services::DockItems::DockItemKind;
using QindaQt::Services::DockItems::DockItems;

namespace {

// A drop event double carrying exactly what a DropArea hands the dock.
class FakeDrop final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList formats MEMBER formats CONSTANT)
  Q_PROPERTY(QVariantList urls MEMBER urls CONSTANT)
  Q_PROPERTY(bool hasUrls READ hasUrls CONSTANT)

public:
  [[nodiscard]] bool hasUrls() const { return !urls.isEmpty(); }
  Q_INVOKABLE QString getDataAsString(const QString &format) const
  {
    return data.value(format);
  }

  QStringList formats;
  QVariantList urls;
  QHash<QString, QString> data;
};

struct DockFixture {
  LauncherStack launcher;
  Shell::Launcher::LauncherAppletController controller{
      &launcher.scanner, &launcher.persistence, &launcher.executor, true};
  RecordingDockPaths paths;
  QuickLaunchController quick{&controller, true};

  explicit DockFixture(const QVector<DockItem> &items)
  {
    QVERIFY(launcher.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Fixture Editor"),
                              QStringLiteral("/bin/true"),
                              QStringLiteral("Categories=Office;\n")));
    QVERIFY(launcher.addEntry(QStringLiteral("sheets.desktop"), QStringLiteral("Fixture Sheets"),
                              QStringLiteral("/bin/true"),
                              QStringLiteral("Categories=Office;\n")));
    QVERIFY(launcher.addEntry(QStringLiteral("terminal.desktop"),
                              QStringLiteral("Fixture Terminal"), QStringLiteral("/bin/true")));
    QVERIFY(launcher.scanner.start());
    launcher.publishDock(*DockItems::fromItems(items));
    quick.setPathPort(&paths);
  }
};

// The live tiles in visual order. Walks the strip's own visual tree only, so
// tiles a rebuilt Repeater has released (and not yet deleted) never count.
QList<QQuickItem *> tiles(const AppletHost &host)
{
  QList<QQuickItem *> result;
  const auto visit = [&result](auto &&self, QQuickItem *item) -> void {
    if (item->objectName() == QLatin1StringView("quickLaunchEntry"))
      result.append(item);
    for (QQuickItem *child : item->childItems())
      self(self, child);
  };
  visit(visit, host.item);
  std::sort(result.begin(), result.end(), [](QQuickItem *left, QQuickItem *right) {
    return left->property("visualIndex").toInt() < right->property("visualIndex").toInt();
  });
  return result;
}

QQuickItem *runningDot(const AppletHost &host, int position)
{
  QQuickItem *tile = tiles(host).value(position);
  return tile != nullptr
      ? tile->findChild<QQuickItem *>(QStringLiteral("quickLaunchRunningIndicator"))
      : nullptr;
}

QObject *gestures(const AppletHost &host)
{
  return host.root->findChild<QObject *>(QStringLiteral("quickLaunchGestures"));
}

// Strip-local centre of tile `position` (DragHandler/DropArea coordinates).
QPointF tileCentre(const AppletHost &host, int position)
{
  QQuickItem *tile = tiles(host).value(position);
  return tile->mapToItem(host.item, QPointF(tile->width() / 2, tile->height() / 2));
}

bool invoke(QObject *object, const char *method, const QVariant &first = {},
            const QVariant &second = {})
{
  if (!first.isValid())
    return QMetaObject::invokeMethod(object, method);
  if (!second.isValid())
    return QMetaObject::invokeMethod(object, method, Q_ARG(QVariant, first));
  return QMetaObject::invokeMethod(object, method, Q_ARG(QVariant, first),
                                   Q_ARG(QVariant, second));
}

} // namespace

class DesktopControlsQmlDockTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void tilesPresentEveryKindWithAccessibleTruth();
  void groupStackUnfoldsKeyboardNavigatesAndCloses();
  void folderStackListsChildrenAndOpensTheFileManager();
  void dragMovesGroupsAndRemovesTiles();
  void outsideDropsOpenALiveGapAndLandAtThePointer();
  void applicationDroppedOnATileMakesAGroup();
  void menuRepeatsGesturesForKeyboardUsers();
  void runningIndicatorIsAlsoSaidInWords();
  void permanentEndsShowOneFixedTileAndOthersLeaveThemOut();
};

void DesktopControlsQmlDockTests::tilesPresentEveryKindWithAccessibleTruth()
{
  DockFixture fixture({DockItem::group(QStringLiteral("Office"),
                                       {QStringLiteral("editor"), QStringLiteral("sheets")}),
                       DockItem::folder(QStringLiteral("/home/fixture/Projects")),
                       DockItem::application(QStringLiteral("terminal")),
                       DockItem::trash()});
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  const auto shown = tiles(host);
  QCOMPARE(shown.size(), 4);

  auto *plate = shown.at(0)->findChild<QQuickItem *>(QStringLiteral("quickLaunchGroupPlate"));
  QVERIFY(plate != nullptr && plate->isVisible());
  QAccessibleInterface *group = QAccessible::queryAccessibleInterface(shown.at(0));
  QVERIFY(group != nullptr);
  QCOMPARE(group->role(), QAccessible::Button);
  QCOMPARE(group->text(QAccessible::Name), QStringLiteral("Office"));
  QVERIFY(group->text(QAccessible::Description).contains(QStringLiteral("2 applications")));
  QCOMPARE(QAccessible::queryAccessibleInterface(shown.at(1))->text(QAccessible::Description),
           QStringLiteral("Folder"));
  QCOMPARE(QAccessible::queryAccessibleInterface(shown.at(3))->text(QAccessible::Name),
           QStringLiteral("Trash"));

  // Left/Right walk the tiles; nothing is reachable only by pointer.
  shown.at(0)->forceActiveFocus(Qt::TabFocusReason);
  keyClickFocused(host, Qt::Key_Right);
  QTRY_VERIFY(shown.at(1)->hasActiveFocus());
  keyClickFocused(host, Qt::Key_Right);
  QTRY_VERIFY(shown.at(2)->hasActiveFocus());
  keyClickFocused(host, Qt::Key_Left);
  QTRY_VERIFY(shown.at(1)->hasActiveFocus());
}

void DesktopControlsQmlDockTests::groupStackUnfoldsKeyboardNavigatesAndCloses()
{
  DockFixture fixture({DockItem::group(QStringLiteral("Office"),
                                       {QStringLiteral("editor"), QStringLiteral("sheets")})});
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  auto *stack = host.child<QObject>(QStringLiteral("quickLaunchStackPopup"));
  QVERIFY(stack != nullptr);
  QCOMPARE(stack->property("popupType").toInt(), PopupTypeWindow);

  tiles(host).constFirst()->forceActiveFocus(Qt::TabFocusReason);
  keyClickFocused(host, Qt::Key_Return);
  QTRY_VERIFY(stack->property("opened").toBool());
  // Unfolding is motion only; the rows exist and take focus at once.
  auto rows = host.visualItemsNamed(QStringLiteral("quickLaunchStackRow"));
  QCOMPARE(rows.size(), 2);
  QTRY_VERIFY(rows.constFirst()->hasActiveFocus());
  keyClickFocused(host, Qt::Key_Down);
  QTRY_VERIFY(rows.at(1)->hasActiveFocus());
  QCOMPARE(QAccessible::queryAccessibleInterface(rows.at(1))->text(QAccessible::Name),
           QStringLiteral("Fixture Sheets"));

  // Escape closes without launching anything.
  keyClickFocused(host, Qt::Key_Escape);
  QTRY_VERIFY(!stack->property("opened").toBool());
  QVERIFY(fixture.launcher.spawner.requests.isEmpty());

  // Choosing a member launches it and folds the stack away.
  keyClickFocused(host, Qt::Key_Return);
  QTRY_VERIFY(stack->property("opened").toBool());
  rows = host.visualItemsNamed(QStringLiteral("quickLaunchStackRow"));
  QTRY_VERIFY(rows.constFirst()->hasActiveFocus());
  keyClickFocused(host, Qt::Key_Return);
  QTRY_COMPARE(fixture.launcher.spawner.requests.size(), 1);
  QTRY_VERIFY(!stack->property("opened").toBool());
}

void DesktopControlsQmlDockTests::folderStackListsChildrenAndOpensTheFileManager()
{
  DockFixture fixture({DockItem::folder(QStringLiteral("/home/fixture/Projects"))});
  fixture.paths.listing = {
      QVariantMap{{QStringLiteral("name"), QStringLiteral("notes")},
                  {QStringLiteral("path"), QStringLiteral("/home/fixture/Projects/notes")},
                  {QStringLiteral("isDirectory"), true},
                  {QStringLiteral("iconName"), QStringLiteral("folder")}},
      QVariantMap{{QStringLiteral("name"), QStringLiteral("plan.txt")},
                  {QStringLiteral("path"), QStringLiteral("/home/fixture/Projects/plan.txt")},
                  {QStringLiteral("isDirectory"), false},
                  {QStringLiteral("iconName"), QStringLiteral("text-plain")}}};
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  auto *stack = host.child<QObject>(QStringLiteral("quickLaunchStackPopup"));
  tiles(host).constFirst()->forceActiveFocus(Qt::TabFocusReason);
  keyClickFocused(host, Qt::Key_Return);
  QTRY_VERIFY(stack->property("opened").toBool());
  QCOMPARE(fixture.paths.listed, QStringList{QStringLiteral("/home/fixture/Projects")});
  QCOMPARE(host.visualItemsNamed(QStringLiteral("quickLaunchStackRow")).size(), 2);

  // Down past the last child reaches "Open in File Manager".
  keyClickFocused(host, Qt::Key_Down);
  keyClickFocused(host, Qt::Key_Down);
  auto *open = host.visualItemsNamed(QStringLiteral("quickLaunchStackOpenInFileManager"))
                   .constFirst();
  QTRY_VERIFY(open->hasActiveFocus());
  keyClickFocused(host, Qt::Key_Space);
  QTRY_COMPARE(fixture.paths.openedFolders,
               QStringList{QStringLiteral("/home/fixture/Projects")});
  QTRY_VERIFY(!stack->property("opened").toBool());
}

void DesktopControlsQmlDockTests::dragMovesGroupsAndRemovesTiles()
{
  DockFixture fixture({DockItem::application(QStringLiteral("editor")),
                       DockItem::application(QStringLiteral("terminal")),
                       DockItem::application(QStringLiteral("sheets"))});
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  QObject *drag = gestures(host);
  QVERIFY(drag != nullptr);
  const qreal slot = host.item->property("slotExtent").toReal();
  const qreal tile = host.item->property("tileExtent").toReal();
  QVERIFY(slot > tile && tile > 0);
  const QPointF y(0, tile / 2);

  // Move: the first tile follows the pointer and the others close its slot
  // and open the gap after the last tile.
  const QPointF press = tileCentre(host, 0);
  QVERIFY(invoke(drag, "dragBegin", press));
  QCOMPARE(drag->property("dragFrom").toInt(), 0);
  const QPointF end(3 * slot - 2, y.y());
  QVERIFY(invoke(drag, "dragUpdate", end, press));
  QCOMPARE(drag->property("hoverGap").toInt(), 3);
  QCOMPARE(drag->property("hoverTarget").toInt(), -1);
  QTRY_COMPARE(tiles(host).at(1)->property("mainShift").toReal(), -slot);
  QVERIFY(tiles(host).at(0)->property("dragHeld").toBool());
  QVERIFY(invoke(drag, "dragEnd"));
  QCOMPARE(fixture.launcher.committedDock().applicationIds(),
           QStringList({QStringLiteral("terminal"), QStringLiteral("sheets"),
                        QStringLiteral("editor")}));
  fixture.launcher.settleDock();
  QTRY_COMPARE(tiles(host).size(), 3);

  // Group: terminal released over the middle of sheets.
  const QPointF terminal = tileCentre(host, 0);
  QVERIFY(invoke(drag, "dragBegin", terminal));
  QVERIFY(invoke(drag, "dragUpdate", tileCentre(host, 1), terminal));
  QCOMPARE(drag->property("hoverTarget").toInt(), 1);
  QTRY_VERIFY(tiles(host).at(1)->property("mergeTarget").toBool());
  QVERIFY(invoke(drag, "dragEnd"));
  const DockItems grouped = fixture.launcher.committedDock();
  QCOMPARE(grouped.size(), 2);
  QCOMPARE(grouped.items().constFirst().kind, DockItemKind::Group);
  QCOMPARE(grouped.items().constFirst().applications,
           QStringList({QStringLiteral("sheets"), QStringLiteral("terminal")}));
  fixture.launcher.settleDock();

  // Remove: editor dragged a whole tile beyond the panel's far edge. The
  // hint says so in words before release.
  QTRY_COMPARE(tiles(host).size(), 2);
  const QPointF editor = tileCentre(host, 1);
  QVERIFY(invoke(drag, "dragBegin", editor));
  QVERIFY(invoke(drag, "dragUpdate", QPointF(editor.x(), -2 * tile), editor));
  QVERIFY(drag->property("dragRemoving").toBool());
  auto *hint = host.child<QQuickItem>(QStringLiteral("quickLaunchRemoveHint"));
  QVERIFY(hint != nullptr);
  QTRY_VERIFY(hint->isVisible());
  QVERIFY(invoke(drag, "dragEnd"));
  QCOMPARE(fixture.launcher.committedDock().applicationIds(),
           QStringList({QStringLiteral("sheets"), QStringLiteral("terminal")}));
  QVERIFY(!hint->isVisible());
}

void DesktopControlsQmlDockTests::outsideDropsOpenALiveGapAndLandAtThePointer()
{
  DockFixture fixture({DockItem::application(QStringLiteral("editor")),
                       DockItem::application(QStringLiteral("terminal"))});
  fixture.paths.directories.insert(QStringLiteral("/home/fixture/Documents"));
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  auto *dropArea = host.child<QQuickItem>(QStringLiteral("quickLaunchDropArea"));
  QVERIFY(dropArea != nullptr);
  QObject *drop = gestures(host);
  const qreal slot = host.item->property("slotExtent").toReal();

  // Hovering between the two tiles opens a gap: each side moves half a slot.
  QVERIFY(invoke(drop, "dropHover", QPointF(slot, 10), QStringLiteral("urls")));
  QCOMPARE(drop->property("hoverGap").toInt(), 1);
  QTRY_COMPARE(tiles(host).at(0)->property("mainShift").toReal(), -slot / 2);
  QTRY_COMPARE(tiles(host).at(1)->property("mainShift").toReal(), slot / 2);

  FakeDrop folder;
  folder.formats = {QStringLiteral("text/uri-list")};
  folder.urls = {QUrl::fromLocalFile(QStringLiteral("/home/fixture/Documents"))};
  QVariant accepted;
  QVERIFY(QMetaObject::invokeMethod(drop, "dropCommit", Q_RETURN_ARG(QVariant, accepted),
                                    Q_ARG(QVariant, QVariant::fromValue<QObject *>(&folder))));
  QVERIFY(accepted.toBool());
  QCOMPARE(fixture.launcher.committedDock().items().at(1),
           DockItem::folder(QStringLiteral("/home/fixture/Documents")));
  // The gap closes once the drop lands.
  QCOMPARE(drop->property("hoverGap").toInt(), -1);
  QTRY_COMPARE(tiles(host).at(0)->property("mainShift").toReal(), 0.0);
  fixture.launcher.settleDock();

  // A drop the dock cannot keep is refused, and the dock is unchanged.
  FakeDrop remote;
  remote.formats = {QStringLiteral("text/uri-list")};
  remote.urls = {QUrl(QStringLiteral("https://example.org/page"))};
  QVERIFY(invoke(drop, "dropHover", QPointF(0, 10), QStringLiteral("urls")));
  const qsizetype commits = fixture.launcher.transport.commits.size();
  QVERIFY(QMetaObject::invokeMethod(drop, "dropCommit", Q_RETURN_ARG(QVariant, accepted),
                                    Q_ARG(QVariant, QVariant::fromValue<QObject *>(&remote))));
  QVERIFY(!accepted.toBool());
  QCOMPARE(fixture.launcher.transport.commits.size(), commits);
  QTRY_VERIFY(fixture.quick.feedbackPresent());
}

void DesktopControlsQmlDockTests::applicationDroppedOnATileMakesAGroup()
{
  DockFixture fixture({DockItem::application(QStringLiteral("editor")),
                       DockItem::application(QStringLiteral("terminal"))});
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  QObject *drop = gestures(host);

  // A launcher row dragged over the middle of Editor targets it for a group.
  QVERIFY(invoke(drop, "dropHover", tileCentre(host, 0), QStringLiteral("application")));
  QCOMPARE(drop->property("hoverTarget").toInt(), 0);
  QTRY_VERIFY(tiles(host).at(0)->property("mergeTarget").toBool());
  FakeDrop sheets;
  sheets.formats = {QStringLiteral("application/x-qindaqt-desktop-entry-id")};
  sheets.data.insert(QStringLiteral("application/x-qindaqt-desktop-entry-id"),
                     QStringLiteral("sheets"));
  QVariant accepted;
  QVERIFY(QMetaObject::invokeMethod(drop, "dropCommit", Q_RETURN_ARG(QVariant, accepted),
                                    Q_ARG(QVariant, QVariant::fromValue<QObject *>(&sheets))));
  QVERIFY(accepted.toBool());
  // Both are Office applications, so the group is named for the category.
  QCOMPARE(fixture.launcher.committedDock().items().constFirst(),
           DockItem::group(QStringLiteral("Office"),
                           {QStringLiteral("editor"), QStringLiteral("sheets")}));
}

void DesktopControlsQmlDockTests::menuRepeatsGesturesForKeyboardUsers()
{
  DockFixture fixture({DockItem::application(QStringLiteral("editor")),
                       DockItem::application(QStringLiteral("terminal"))});
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  auto *menu = host.child<QObject>(QStringLiteral("quickLaunchContextMenu"));
  QVERIFY(menu != nullptr);
  QCOMPARE(menu->property("popupType").toInt(), PopupTypeWindow);

  // The Menu key opens the tile's menu; Move right trades places and the
  // moved tile keeps keyboard focus.
  tiles(host).constFirst()->forceActiveFocus(Qt::TabFocusReason);
  keyClickFocused(host, Qt::Key_Menu);
  QTRY_VERIFY(menu->property("opened").toBool());
  auto *moveRight = host.child<QObject>(QStringLiteral("quickLaunchMoveDown"));
  QVERIFY(moveRight != nullptr);
  QCOMPARE(moveRight->property("text").toString(), QStringLiteral("Move right"));
  QVERIFY(QMetaObject::invokeMethod(moveRight, "triggered"));
  QCOMPARE(fixture.launcher.committedDock().applicationIds(),
           QStringList({QStringLiteral("terminal"), QStringLiteral("editor")}));
  fixture.launcher.settleDock();
  QTRY_VERIFY(tiles(host).at(1)->hasActiveFocus());

  // New Group asks for a name, suggesting the application's category.
  menu->setProperty("row", fixture.quick.rows().at(1));
  menu->setProperty("visualIndex", 1);
  menu->setProperty("anchorTile", QVariant::fromValue(tiles(host).at(1)));
  QVERIFY(QMetaObject::invokeMethod(host.child<QObject>(QStringLiteral("quickLaunchNewGroup")),
                                    "triggered"));
  auto *prompt = host.child<QObject>(QStringLiteral("quickLaunchPromptPopup"));
  QVERIFY(prompt != nullptr);
  QTRY_VERIFY(prompt->property("opened").toBool());
  auto *name = host.visualItemsNamed(QStringLiteral("quickLaunchPromptName")).constFirst();
  QCOMPARE(name->property("text").toString(), QStringLiteral("Office"));
  QTRY_VERIFY(name->hasActiveFocus());
  keyClickFocused(host, Qt::Key_Return);
  QTRY_VERIFY(!prompt->property("opened").toBool());
  QCOMPARE(fixture.launcher.committedDock().items().at(1),
           DockItem::group(QStringLiteral("Office"), {QStringLiteral("editor")}));
  fixture.launcher.settleDock();

  // Remove from Dock is the keyboard's drag-off.
  menu->setProperty("row", fixture.quick.rows().at(0));
  QVERIFY(QMetaObject::invokeMethod(host.child<QObject>(QStringLiteral("quickLaunchUnpin")),
                                    "triggered"));
  QCOMPARE(fixture.launcher.committedDock().size(), 1);
}

void DesktopControlsQmlDockTests::runningIndicatorIsAlsoSaidInWords()
{
  DockFixture fixture({DockItem::application(QStringLiteral("editor"))});
  TaskListStack tasks;
  ShellTaskListApplet::TaskListAppletController taskList(tasks.source, tasks.authority,
                                                         tasks.port, {true, true, true});
  fixture.quick.setWindowSource(
      &taskList,
      [](const QString &applicationId) {
        return applicationId == QLatin1StringView("org.qindaqt.TextEditor")
            ? QStringLiteral("editor") : QString{};
      },
      {true, true});
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  QVERIFY(runningDot(host, 0) != nullptr);
  QVERIFY(!runningDot(host, 0)->isVisible());

  QVERIFY(tasks.publish(TaskListStack::activeEditorFacts()) > 0);
  QTRY_VERIFY(runningDot(host, 0) != nullptr && runningDot(host, 0)->isVisible());
  QVERIFY(QAccessible::queryAccessibleInterface(tiles(host).constFirst())
              ->text(QAccessible::Description).startsWith(QStringLiteral("Running")));
}

// ADR-0268: the Mac-style dock's slices. "others" leaves the stored Trash
// to the Trash end; the end is one fixed tile its menu never moves or
// removes, and it opens the Trash without a stored index.
void DesktopControlsQmlDockTests::permanentEndsShowOneFixedTileAndOthersLeaveThemOut()
{
  DockFixture fixture({DockItem::application(QStringLiteral("editor")), DockItem::trash()});
  AppletHost host;
  QString error;
  QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &fixture.quick, &error, false,
                       true, 60, 60), qPrintable(error));
  QTRY_VERIFY(host.window->isExposed());
  QCOMPARE(tiles(host).size(), 2);
  host.root->setProperty("items", QStringLiteral("others"));
  QTRY_COMPARE(tiles(host).size(), 1);
  QCOMPARE(QAccessible::queryAccessibleInterface(tiles(host).constFirst())->text(QAccessible::Name),
           QStringLiteral("Fixture Editor"));
  // No File Manager in this catalog: its end shows nothing rather than a gap.
  host.root->setProperty("items", QStringLiteral("file-manager"));
  QTRY_COMPARE(tiles(host).size(), 0);

  host.root->setProperty("items", QStringLiteral("trash"));
  QTRY_COMPARE(tiles(host).size(), 1);
  auto *menu = host.child<QObject>(QStringLiteral("quickLaunchContextMenu"));
  QVERIFY(menu != nullptr);
  tiles(host).constFirst()->forceActiveFocus(Qt::TabFocusReason);
  keyClickFocused(host, Qt::Key_Menu);
  QTRY_VERIFY(menu->property("opened").toBool());
  QVERIFY(menu->property("fixed").toBool());
  for (const char *hidden : {"quickLaunchUnpin", "quickLaunchMoveUp", "quickLaunchMoveDown",
                             "quickLaunchShowTrash"}) {
    QVERIFY2(!host.child<QObject>(QString::fromLatin1(hidden))->property("visible").toBool(),
             hidden);
  }
  QVERIFY(host.child<QObject>(QStringLiteral("quickLaunchEmptyTrash"))->property("visible").toBool());
  fixture.paths.directories.insert(fixture.paths.trash);
  QVERIFY(QMetaObject::invokeMethod(host.child<QObject>(QStringLiteral("quickLaunchOpen")),
                                    "triggered"));
  QCOMPARE(fixture.paths.openedFolders.constLast(), fixture.paths.trash);
  QVERIFY(fixture.launcher.transport.commits.isEmpty());
}

QTEST_MAIN(DesktopControlsQmlDockTests)
#include "tst_desktop_controls_qml_dock.moc"
