// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0265: the dock facade over the launcher's dock value — row projection,
// drops, groups, removal, path items through the File Manager seam, running
// indicators with one icon per application, and Keep in Dock.
#include "desktop_controls_test_support.h"

#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"

#include <QUrl>
#include <QtTest>

using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;
using QindaQt::Services::DockItems::DockItem;
using QindaQt::Services::DockItems::DockItemKind;
using QindaQt::Services::DockItems::DockItems;
using QindaQt::ShellTaskListApplet::TaskListAppletController;

namespace {

// A stack with three installed applications and a confirmed dock.
struct DockStack {
  LauncherStack launcher;
  QindaQt::Shell::Launcher::LauncherAppletController controller{
      &launcher.scanner, &launcher.persistence, &launcher.executor, true};

  explicit DockStack(const DockItems &dock)
  {
    QVERIFY(launcher.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Fixture Editor"),
                              QStringLiteral("/bin/true"),
                              QStringLiteral("Icon=accessories-text-editor\nCategories=Office;\n")));
    QVERIFY(launcher.addEntry(QStringLiteral("sheets.desktop"), QStringLiteral("Fixture Sheets"),
                              QStringLiteral("/bin/true"),
                              QStringLiteral("Icon=x-office-spreadsheet\nCategories=Office;\n")));
    QVERIFY(launcher.addEntry(QStringLiteral("terminal.desktop"),
                              QStringLiteral("Fixture Terminal"), QStringLiteral("/bin/true")));
    QVERIFY(launcher.scanner.start());
    launcher.publishDock(dock);
  }
};

DockItems dockOf(const QVector<DockItem> &items)
{
  return *DockItems::fromItems(items);
}

QVariantMap rowAt(const QuickLaunchController &quick, int position)
{
  return quick.rows().value(position).toMap();
}

} // namespace

class DockControllerTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void rowsProjectEveryKindWithStoredIndices();
  void dropsInsertAtTheGapAndIgnoreWhatIsNotLocal();
  void desktopEntryDropsBecomeTheirApplication();
  void groupsCombineRenameUngroupAndReleaseMembers();
  void removalAndMovesEditTheStoredDock();
  void pathItemsOpenOnlyThroughThePathPort();
  void emptyTrashSettlesThroughThePort();
  void runningPinnedApplicationsClaimTheirWindows();
  void keepInDockPinsARunningWindowsApplication();
  void refusalsAreFeedbackNotSilence();
};

void DockControllerTests::rowsProjectEveryKindWithStoredIndices()
{
  DockStack stack(dockOf({DockItem::group(QStringLiteral("Office"),
                                          {QStringLiteral("editor"), QStringLiteral("sheets")}),
                          DockItem::application(QStringLiteral("uninstalled")),
                          DockItem::folder(QStringLiteral("/home/fixture/Projects")),
                          DockItem::application(QStringLiteral("terminal")),
                          DockItem::file(QStringLiteral("/home/fixture/plan.pdf")),
                          DockItem::trash()}));
  QuickLaunchController quick(&stack.controller, true);

  // The uninstalled application keeps its stored place but shows nothing.
  QCOMPARE(quick.itemCount(), 6);
  QCOMPARE(quick.count(), 5);
  const QVariantMap group = rowAt(quick, 0);
  QCOMPARE(group.value(QStringLiteral("kind")).toString(), QStringLiteral("group"));
  QCOMPARE(group.value(QStringLiteral("index")).toInt(), 0);
  QCOMPARE(group.value(QStringLiteral("displayText")).toString(), QStringLiteral("Office"));
  QCOMPARE(group.value(QStringLiteral("members")).toList().size(), 2);
  QCOMPARE(group.value(QStringLiteral("previewIcons")).toStringList(),
           QStringList({QStringLiteral("accessories-text-editor"),
                        QStringLiteral("x-office-spreadsheet")}));
  const QVariantMap member = group.value(QStringLiteral("members")).toList().constFirst().toMap();
  QCOMPARE(member.value(QStringLiteral("entryId")).toString(), QStringLiteral("editor"));
  QCOMPARE(member.value(QStringLiteral("categoryIdentity")).toString(), QStringLiteral("office"));

  const QVariantMap folder = rowAt(quick, 1);
  QCOMPARE(folder.value(QStringLiteral("kind")).toString(), QStringLiteral("folder"));
  QCOMPARE(folder.value(QStringLiteral("index")).toInt(), 2); // stored index, not position
  QCOMPARE(folder.value(QStringLiteral("displayText")).toString(), QStringLiteral("Projects"));
  QCOMPARE(folder.value(QStringLiteral("iconName")).toString(), QStringLiteral("folder"));
  QCOMPARE(rowAt(quick, 2).value(QStringLiteral("entryId")).toString(),
           QStringLiteral("terminal"));
  QCOMPARE(rowAt(quick, 3).value(QStringLiteral("kind")).toString(), QStringLiteral("file"));
  QCOMPARE(rowAt(quick, 4).value(QStringLiteral("kind")).toString(), QStringLiteral("trash"));
  QVERIFY(quick.trashInDock());

  // Application tiles see applications only, group members in place.
  QStringList applications;
  for (const QVariant &row : quick.applicationRows())
    applications.append(row.toMap().value(QStringLiteral("entryId")).toString());
  QCOMPARE(applications, QStringList({QStringLiteral("editor"), QStringLiteral("sheets"),
                                      QStringLiteral("terminal")}));
  // The launcher's Pinned section is the same applications.
  QCOMPARE(stack.launcher.persistence.pinned().ids(),
           QStringList({QStringLiteral("editor"), QStringLiteral("sheets"),
                        QStringLiteral("uninstalled"), QStringLiteral("terminal")}));
}

void DockControllerTests::dropsInsertAtTheGapAndIgnoreWhatIsNotLocal()
{
  DockStack stack(dockOf({DockItem::application(QStringLiteral("editor")),
                          DockItem::application(QStringLiteral("terminal"))}));
  RecordingDockPaths paths;
  paths.directories.insert(QStringLiteral("/home/fixture/Documents"));
  paths.files.insert(QStringLiteral("/home/fixture/notes.txt"));
  QuickLaunchController quick(&stack.controller, true);
  quick.setPathPort(&paths);
  QVERIFY(quick.editable());

  QVERIFY(quick.insertUrls(1, {QUrl::fromLocalFile(QStringLiteral("/home/fixture/Documents")),
                               QUrl(QStringLiteral("https://example.org/remote")),
                               QUrl::fromLocalFile(QStringLiteral("/home/fixture/missing")),
                               QUrl::fromLocalFile(QStringLiteral("/home/fixture/notes.txt"))}));
  const DockItems written = stack.launcher.committedDock();
  QCOMPARE(written.size(), 4);
  QCOMPARE(written.items().at(1), DockItem::folder(QStringLiteral("/home/fixture/Documents")));
  QCOMPARE(written.items().at(2), DockItem::file(QStringLiteral("/home/fixture/notes.txt")));
  QCOMPARE(written.items().at(3).applicationId, QStringLiteral("terminal"));
  // The live dock shows the drop before Settings1 confirms it.
  QCOMPARE(quick.count(), 4);
  stack.launcher.settleDock();

  // The Trash folder itself becomes the Trash item.
  paths.directories.insert(paths.trash);
  QVERIFY(quick.insertUrls(quick.itemCount(), {QUrl::fromLocalFile(paths.trash)}));
  QCOMPARE(stack.launcher.committedDock().items().constLast().kind, DockItemKind::Trash);
  stack.launcher.settleDock();

  // Nothing local in the drop: refused with feedback, nothing written.
  const qsizetype commits = stack.launcher.transport.commits.size();
  QVERIFY(!quick.insertUrls(0, {QUrl(QStringLiteral("sftp://host/file"))}));
  QVERIFY(quick.feedbackPresent());
  QCOMPARE(stack.launcher.transport.commits.size(), commits);
}

void DockControllerTests::desktopEntryDropsBecomeTheirApplication()
{
  DockStack stack(dockOf({}));
  RecordingDockPaths paths;
  paths.files.insert(QStringLiteral("/usr/share/applications/sheets.desktop"));
  paths.files.insert(QStringLiteral("/home/fixture/Desktop/unknown.desktop"));
  QuickLaunchController quick(&stack.controller, true);
  quick.setPathPort(&paths);

  // A desktop entry names an installed application; its text is never read.
  QVERIFY(quick.insertUrls(0, {QUrl::fromLocalFile(
                                   QStringLiteral("/usr/share/applications/sheets.desktop"))}));
  QCOMPARE(stack.launcher.committedDock().items().constFirst(),
           DockItem::application(QStringLiteral("sheets")));
  stack.launcher.settleDock();
  // One that names nothing installed stays a plain file item.
  QVERIFY(quick.insertUrls(1, {QUrl::fromLocalFile(
                                   QStringLiteral("/home/fixture/Desktop/unknown.desktop"))}));
  QCOMPARE(stack.launcher.committedDock().items().constLast().kind, DockItemKind::File);
}

void DockControllerTests::groupsCombineRenameUngroupAndReleaseMembers()
{
  DockStack stack(dockOf({DockItem::application(QStringLiteral("editor")),
                          DockItem::application(QStringLiteral("terminal")),
                          DockItem::application(QStringLiteral("sheets"))}));
  QuickLaunchController quick(&stack.controller, true);
  QCOMPARE(quick.applicationCategory(QStringLiteral("sheets")), QStringLiteral("office"));

  // Dropping Sheets onto Editor makes a group in Editor's place.
  QVERIFY(quick.combineItems(0, 2, QStringLiteral("  Office  ")));
  QCOMPARE(stack.launcher.committedDock().items().constFirst(),
           DockItem::group(QStringLiteral("Office"),
                           {QStringLiteral("editor"), QStringLiteral("sheets")}));
  stack.launcher.settleDock();
  QCOMPARE(quick.count(), 2);

  QVERIFY(quick.renameGroup(0, QStringLiteral("Work")));
  QCOMPARE(stack.launcher.committedDock().items().constFirst().name, QStringLiteral("Work"));
  stack.launcher.settleDock();

  // A member dragged back out lands at the pointer's gap; the group stays.
  QVERIFY(quick.moveOutOfGroup(0, QStringLiteral("sheets"), 2));
  QCOMPARE(stack.launcher.committedDock().applicationIds(),
           QStringList({QStringLiteral("editor"), QStringLiteral("terminal"),
                        QStringLiteral("sheets")}));
  stack.launcher.settleDock();

  // New Group wraps one application; Ungroup returns its members.
  QVERIFY(quick.newGroup(1, QStringLiteral("Tools")));
  stack.launcher.settleDock();
  QCOMPARE(rowAt(quick, 1).value(QStringLiteral("kind")).toString(), QStringLiteral("group"));
  QVERIFY(quick.ungroup(0));
  stack.launcher.settleDock();
  QCOMPARE(rowAt(quick, 0).value(QStringLiteral("kind")).toString(),
           QStringLiteral("application"));
  QCOMPARE(quick.itemCount(), 3);
}

void DockControllerTests::removalAndMovesEditTheStoredDock()
{
  DockStack stack(dockOf({DockItem::application(QStringLiteral("editor")),
                          DockItem::folder(QStringLiteral("/srv")),
                          DockItem::application(QStringLiteral("terminal"))}));
  QuickLaunchController quick(&stack.controller, true);
  QVERIFY(quick.moveItem(0, 3));
  QCOMPARE(stack.launcher.committedDock().items().constLast().applicationId,
           QStringLiteral("editor"));
  stack.launcher.settleDock();
  QVERIFY(quick.removeItem(0)); // the folder
  QCOMPARE(stack.launcher.committedDock().size(), 2);
  stack.launcher.settleDock();
  // The keyboard move actions keep working on applications.
  QVERIFY(quick.moveUp(QStringLiteral("editor")));
  QCOMPARE(stack.launcher.committedDock().items().constFirst().applicationId,
           QStringLiteral("editor"));
  stack.launcher.settleDock();
  QVERIFY(quick.unpin(QStringLiteral("editor")));
  QCOMPARE(stack.launcher.committedDock().applicationIds(),
           QStringList{QStringLiteral("terminal")});
}

void DockControllerTests::pathItemsOpenOnlyThroughThePathPort()
{
  DockStack stack(dockOf({DockItem::folder(QStringLiteral("/home/fixture/Projects")),
                          DockItem::file(QStringLiteral("/home/fixture/plan.pdf")),
                          DockItem::trash()}));
  RecordingDockPaths paths;
  paths.listing = {QVariantMap{{QStringLiteral("name"), QStringLiteral("a.txt")},
                               {QStringLiteral("path"), QStringLiteral("/home/fixture/Projects/a.txt")},
                               {QStringLiteral("isDirectory"), false}}};
  QuickLaunchController quick(&stack.controller, true);
  quick.setPathPort(&paths);

  QVERIFY(quick.activateItem(1));
  QCOMPARE(paths.openedFiles, QStringList{QStringLiteral("/home/fixture/plan.pdf")});
  QVERIFY(quick.openInFileManager(0));
  QCOMPARE(paths.openedFolders, QStringList{QStringLiteral("/home/fixture/Projects")});
  const QVariantList entries = quick.folderEntries(0);
  QCOMPARE(entries.size(), 1);
  QCOMPARE(paths.listed, QStringList{QStringLiteral("/home/fixture/Projects")});
  QVERIFY(quick.openFolderEntry(entries.constFirst().toMap()));
  QCOMPARE(paths.openedEntries.size(), 1);

  // An empty Trash has no folder yet: said, not opened.
  QVERIFY(!quick.activateItem(2));
  QVERIFY(quick.feedback().contains(QStringLiteral("empty")));
  paths.directories.insert(paths.trash);
  QVERIFY(quick.activateItem(2));
  QCOMPARE(paths.openedFolders.constLast(), paths.trash);

  // A refused open keeps its reason visible; no other program is tried.
  paths.nextResult = {false, QStringLiteral("the QindaQt File Manager is not installed")};
  QVERIFY(!quick.openInFileManager(0));
  QVERIFY(quick.feedback().contains(QStringLiteral("not installed")));
  QCOMPARE(stack.launcher.spawner.requests.size(), 0);
  // Folders never list or open for a non-folder row.
  QVERIFY(quick.folderEntries(1).isEmpty());
}

void DockControllerTests::emptyTrashSettlesThroughThePort()
{
  DockStack stack(dockOf({DockItem::trash()}));
  RecordingDockPaths paths;
  QuickLaunchController quick(&stack.controller, true);
  quick.setPathPort(&paths);

  QVERIFY(quick.emptyTrash());
  QCOMPARE(paths.emptyTrashCalls, 1);
  paths.trashFinished(false, QStringLiteral("permission denied"));
  QVERIFY(quick.feedback().contains(QStringLiteral("permission denied")));
  QVERIFY(quick.emptyTrash());
  paths.trashFinished(true, {});
  QVERIFY(!quick.feedbackPresent());
  paths.trashStarts = false;
  QVERIFY(!quick.emptyTrash());
  QVERIFY(quick.feedback().contains(QStringLiteral("trash refused")));
}

void DockControllerTests::runningPinnedApplicationsClaimTheirWindows()
{
  DockStack stack(dockOf({DockItem::application(QStringLiteral("editor")),
                          DockItem::group(QStringLiteral("Office"),
                                          {QStringLiteral("sheets")})}));
  TaskListStack tasks;
  TaskListAppletController taskList(tasks.source, tasks.authority, tasks.port,
                                    {true, true, true});
  QuickLaunchController quick(&stack.controller, true);
  quick.setWindowSource(
      &taskList,
      [](const QString &applicationId) {
        return applicationId == QLatin1StringView("org.qindaqt.TextEditor")
            ? QStringLiteral("editor") : QString{};
      },
      {true, true});
  QVERIFY(tasks.publish(TaskListStack::activeEditorFacts()) > 0);

  const QVariantMap editor = rowAt(quick, 0);
  QVERIFY(editor.value(QStringLiteral("running")).toBool());
  QVERIFY(!rowAt(quick, 1).value(QStringLiteral("running")).toBool());
  // One icon per application: the dock tile stands for the editor's window.
  QCOMPARE(quick.claimedTaskIds(), QStringList{QStringLiteral("w-editor")});

  // Activating a running pinned application switches to it; nothing starts.
  QVERIFY(quick.activate(QStringLiteral("editor")));
  QCOMPARE(tasks.port.calls.size(), 1);
  QCOMPARE(tasks.port.lastCall().firstId, QStringLiteral("w-editor"));
  QVERIFY(stack.launcher.spawner.requests.isEmpty());
  // Open New Window always starts another instance.
  QVERIFY(quick.openNewWindow(QStringLiteral("editor")));
  QCOMPARE(stack.launcher.spawner.requests.size(), 1);

  // Without window grants there are no indicators and no claims.
  QuickLaunchController blind(&stack.controller, true);
  blind.setWindowSource(&taskList, {}, {false, false});
  QVERIFY(!blind.rows().constFirst().toMap().value(QStringLiteral("running")).toBool());
  QVERIFY(blind.claimedTaskIds().isEmpty());
}

void DockControllerTests::keepInDockPinsARunningWindowsApplication()
{
  DockStack stack(dockOf({DockItem::application(QStringLiteral("editor"))}));
  QuickLaunchController quick(&stack.controller, true);
  quick.setWindowSource(nullptr,
                        [](const QString &applicationId) {
                          return applicationId == QLatin1StringView("org.qindaqt.Terminal")
                              ? QStringLiteral("terminal") : QString{};
                        },
                        {});
  QVERIFY(quick.canKeepInDock(QStringLiteral("org.qindaqt.Terminal")));
  QVERIFY(!quick.canKeepInDock(QStringLiteral("editor"))); // already kept
  QVERIFY(!quick.canKeepInDock(QStringLiteral("org.unknown.App")));
  QVERIFY(quick.keepInDock(QStringLiteral("org.qindaqt.Terminal")));
  QCOMPARE(stack.launcher.committedDock().applicationIds(),
           QStringList({QStringLiteral("editor"), QStringLiteral("terminal")}));
}

void DockControllerTests::refusalsAreFeedbackNotSilence()
{
  DockStack stack(dockOf({DockItem::application(QStringLiteral("editor"))}));
  QuickLaunchController quick(&stack.controller, true);
  QVERIFY(!quick.insertApplication(0, QStringLiteral("not.installed")));
  QVERIFY(quick.feedback().contains(QStringLiteral("not installed")));
  QVERIFY(!quick.insertApplication(0, QStringLiteral("editor")));
  QVERIFY(quick.feedback().contains(QStringLiteral("already in the Dock")));

  // One write at a time: a second edit while the first saves is refused.
  QVERIFY(quick.insertApplication(1, QStringLiteral("terminal")));
  QVERIFY(!quick.editable());
  QVERIFY(!quick.removeItem(0));
  QVERIFY(quick.feedback().contains(QStringLiteral("still saving")));

  QuickLaunchController detached(nullptr, true);
  QVERIFY(!detached.editable());
  QVERIFY(!detached.moveItem(0, 1));
  QVERIFY(detached.feedback().contains(QStringLiteral("unavailable")));
}

QTEST_GUILESS_MAIN(DockControllerTests)
#include "tst_dock_controller.moc"
