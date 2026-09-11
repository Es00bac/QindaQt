// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_controls_test_support.h"

#include "qindaqt/shell/desktop_controls/active_application_controller.h"
#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"

#include <qindaqt/shell/icons/desktop_entry_icon_resolver.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;
using QindaQt::ShellTaskListApplet::TaskListAppletController;

class QuickLaunchAndActiveApplicationTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void quickLaunchMirrorsPinnedEntriesIndependentlyOfTheLauncherQuery();
  void quickLaunchRefusesUnknownOrUngrantedActivation();
  void activeApplicationFollowsTheActiveTaskRow();
  void activeApplicationIntentsAreFencedByGrantsAndPending();
  void activeApplicationAndTaskRowsShowDesktopEntryNames();
};

namespace {

QVariantMap rowForTask(const TaskListAppletController &taskList, const QString &taskId)
{
  for (const QVariant &value : taskList.entryRows()) {
    const QVariantMap row = value.toMap();
    if (row.value(QStringLiteral("taskId")).toString() == taskId) {
      return row;
    }
  }
  return {};
}

} // namespace

void QuickLaunchAndActiveApplicationTests::activeApplicationAndTaskRowsShowDesktopEntryNames()
{
  // The composition's resolver maps raw compositor ids to desktop-entry
  // names; the task rows carry the result, so the active application
  // indicator, its accessible name, and the task buttons agree.
  QTemporaryDir applications;
  QVERIFY(applications.isValid());
  QFile editorEntry(applications.filePath(QStringLiteral("org.qindaqt.TextEditor.desktop")));
  QVERIFY(editorEntry.open(QIODevice::WriteOnly));
  QVERIFY(editorEntry.write("[Desktop Entry]\nType=Application\nName=Text Editor\n"
                            "Icon=accessories-text-editor\n") > 0);
  editorEntry.close();
  const QindaQt::Shell::Icons::DesktopEntryIconResolver resolver({applications.path()});

  TaskListStack tasks;
  TaskListAppletController taskList(
      tasks.source, tasks.authority, tasks.port, {true, true, true}, {}, {},
      [&resolver](const QString &applicationId, const QString &reportedName) {
        return resolver.applicationDisplayName(applicationId, reportedName);
      });
  ActiveApplicationController active(&taskList, {true, true});
  auto facts = TaskListStack::activeEditorFacts();
  QVERIFY(tasks.publish(facts) > 0);
  QCOMPARE(active.applicationId(), QStringLiteral("org.qindaqt.TextEditor"));
  QCOMPARE(active.applicationName(), QStringLiteral("Text Editor"));
  QCOMPARE(active.accessibleName(), QStringLiteral("Active application: Text Editor"));
  const QVariantMap editorRow = rowForTask(taskList, QStringLiteral("w-editor"));
  QCOMPARE(editorRow.value(QStringLiteral("applicationName")).toString(),
           QStringLiteral("Text Editor"));
  QVERIFY(editorRow.value(QStringLiteral("accessibleName")).toString()
              .startsWith(QStringLiteral("Text Editor")));

  // No desktop entry and a Wayland window reporting its raw app id: the
  // prettified id, never "org.qindaqt.Terminal".
  facts[0].active = false;
  facts[1].active = true;
  facts[1].minimized = false;
  facts[1].applicationName = facts[1].applicationId;
  QVERIFY(tasks.publish(facts) > 0);
  QCOMPARE(active.taskId(), QStringLiteral("w-terminal"));
  QCOMPARE(active.applicationName(), QStringLiteral("Terminal"));
  QCOMPARE(active.accessibleName(), QStringLiteral("Active application: Terminal"));
  const QVariantMap terminalRow = rowForTask(taskList, QStringLiteral("w-terminal"));
  QCOMPARE(terminalRow.value(QStringLiteral("applicationName")).toString(),
           QStringLiteral("Terminal"));
  QVERIFY(!terminalRow.value(QStringLiteral("accessibleName")).toString()
               .contains(QStringLiteral("org.qindaqt")));
}

void QuickLaunchAndActiveApplicationTests::
    quickLaunchMirrorsPinnedEntriesIndependentlyOfTheLauncherQuery()
{
  LauncherStack stack;
  QVERIFY(stack.root.isValid());
  QVERIFY(stack.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Fixture Editor"),
                         QStringLiteral("/bin/true"), QStringLiteral("Icon=accessories-text-editor\n")));
  QVERIFY(stack.addEntry(QStringLiteral("terminal.desktop"), QStringLiteral("Fixture Terminal"),
                         QStringLiteral("/bin/true")));
  QVERIFY(stack.scanner.start());
  stack.publishPinned({QStringLiteral("terminal"), QStringLiteral("editor")});
  QindaQt::Shell::Launcher::LauncherAppletController launcher(
      &stack.scanner, &stack.persistence, &stack.executor, true);

  QuickLaunchController quickLaunch(&launcher, true);
  QVERIFY(quickLaunch.available());
  QCOMPARE(quickLaunch.phaseText(), QStringLiteral("ready"));
  QCOMPARE(quickLaunch.count(), 2);
  const QVariantMap first = quickLaunch.rows().constFirst().toMap();
  QCOMPARE(first.value(QStringLiteral("entryId")).toString(), QStringLiteral("terminal"));
  QCOMPARE(first.value(QStringLiteral("index")).toInt(), 0);
  const QVariantMap second = quickLaunch.rows().constLast().toMap();
  QCOMPARE(second.value(QStringLiteral("displayText")).toString(), QStringLiteral("Fixture Editor"));
  QCOMPARE(second.value(QStringLiteral("iconName")).toString(),
           QStringLiteral("accessories-text-editor"));
  QCOMPARE(second.value(QStringLiteral("accessibleName")).toString(),
           QStringLiteral("Fixture Editor"));

  // AGENT-NOTE (regression): typing in the launcher popup collapses the
  // launcher's own sections to search results; the pinned strip must not
  // follow that query.
  launcher.setQuery(QStringLiteral("zzz-no-match"));
  QCOMPARE(quickLaunch.count(), 2);

  QVERIFY(quickLaunch.activate(QStringLiteral("editor")));
  QCOMPARE(stack.spawner.requests.size(), 1);
  QVERIFY(!quickLaunch.feedbackPresent());
}

void QuickLaunchAndActiveApplicationTests::quickLaunchRefusesUnknownOrUngrantedActivation()
{
  LauncherStack stack;
  QVERIFY(stack.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Fixture Editor"),
                         QStringLiteral("/bin/true")));
  QVERIFY(stack.scanner.start());
  stack.publishPinned({QStringLiteral("editor")});
  QindaQt::Shell::Launcher::LauncherAppletController launcher(
      &stack.scanner, &stack.persistence, &stack.executor, true);

  QuickLaunchController granted(&launcher, true);
  QVERIFY(!granted.activate(QStringLiteral("terminal"))); // not pinned
  QVERIFY(granted.feedback().contains(QStringLiteral("no longer pinned")));
  QVERIFY(stack.spawner.requests.isEmpty());
  QVERIFY(!granted.unpin(QStringLiteral("terminal")));
  QVERIFY(!granted.moveUp(QStringLiteral("terminal")));
  granted.clearFeedback();
  QVERIFY(!granted.feedbackPresent());

  QuickLaunchController ungranted(&launcher, false);
  QVERIFY(!ungranted.available());
  QCOMPARE(ungranted.count(), 1); // rows stay visible, activation refuses
  QVERIFY(!ungranted.activate(QStringLiteral("editor")));
  QVERIFY(ungranted.feedback().contains(QStringLiteral("not granted")));
  QVERIFY(stack.spawner.requests.isEmpty());

  QuickLaunchController detached(nullptr, true);
  QCOMPARE(detached.phaseText(), QStringLiteral("unavailable"));
  QCOMPARE(detached.count(), 0);
  QVERIFY(!detached.activate(QStringLiteral("editor")));
}

void QuickLaunchAndActiveApplicationTests::activeApplicationFollowsTheActiveTaskRow()
{
  TaskListStack tasks;
  TaskListAppletController taskList(tasks.source, tasks.authority, tasks.port,
                                    {true, true, true});
  ActiveApplicationController active(&taskList, {true, true});
  QSignalSpy stateSpy(&active, &ActiveApplicationController::stateChanged);
  QVERIFY(active.available());
  QVERIFY(!active.hasActiveWindow());
  QCOMPARE(active.accessibleName(), QStringLiteral("No active application"));
  QVERIFY(!active.canManage());

  const quint64 revision = tasks.publish(TaskListStack::activeEditorFacts());
  QVERIFY(revision > 0);
  QVERIFY(active.hasActiveWindow());
  QCOMPARE(active.taskId(), QStringLiteral("w-editor"));
  QCOMPARE(active.applicationId(), QStringLiteral("org.qindaqt.TextEditor"));
  QCOMPARE(active.applicationName(), QStringLiteral("App org.qindaqt.TextEditor"));
  QCOMPARE(active.title(), QStringLiteral("Title w-editor"));
  QCOMPARE(active.revision(), revision);
  QCOMPARE(active.windowCount(), 1);
  QVERIFY(!active.minimized());
  QVERIFY(active.canManage());
  QVERIFY(active.accessibleName().contains(QStringLiteral("App org.qindaqt.TextEditor")));
  QVERIFY(stateSpy.size() >= 1);

  // Focus moves: the indicator follows the new active row.
  auto facts = TaskListStack::activeEditorFacts();
  facts[0].active = false;
  facts[1].active = true;
  facts[1].minimized = false;
  tasks.publish(facts);
  QCOMPARE(active.taskId(), QStringLiteral("w-terminal"));

  // No active row at all: truthful absence.
  facts[1].active = false;
  tasks.publish(facts);
  QVERIFY(!active.hasActiveWindow());
  QCOMPARE(active.accessibleDescription(), QStringLiteral("No window is focused"));
}

void QuickLaunchAndActiveApplicationTests::activeApplicationIntentsAreFencedByGrantsAndPending()
{
  TaskListStack tasks;
  TaskListAppletController taskList(tasks.source, tasks.authority, tasks.port,
                                    {true, true, true});
  ActiveApplicationController active(&taskList, {true, true});
  tasks.publish(TaskListStack::activeEditorFacts());

  QVERIFY(active.minimize());
  QCOMPARE(tasks.port.calls.size(), 1);
  QCOMPARE(tasks.port.lastCall().method, QStringLiteral("executeTaskIntent"));
  QCOMPARE(tasks.port.lastCall().firstId, QStringLiteral("w-editor"));
  QVERIFY(active.pending());
  QVERIFY(!active.canManage());
  QVERIFY(!active.close()); // pending fence
  QCOMPARE(tasks.port.calls.size(), 1);
  QVERIFY(active.feedback().contains(QStringLiteral("pending")));
  tasks.port.complete(tasks.port.lastCall(),
                      QindaQt::ShellTaskList::Operations::TaskListOperationStatus::Committed,
                      QStringLiteral("ok"), {});
  QTRY_VERIFY(!active.pending());

  QVERIFY(active.close());
  QCOMPARE(tasks.port.calls.size(), 2);
  tasks.port.complete(tasks.port.lastCall(),
                      QindaQt::ShellTaskList::Operations::TaskListOperationStatus::Rejected,
                      QStringLiteral("refused"), QStringLiteral("compositor refused"));
  QTRY_VERIFY(active.feedbackPresent());
  active.clearFeedback();

  ActiveApplicationController readOnly(&taskList, {true, false});
  QVERIFY(readOnly.hasActiveWindow());
  QVERIFY(!readOnly.canManage());
  QVERIFY(!readOnly.minimize());
  QVERIFY(readOnly.feedback().contains(QStringLiteral("not granted")));
  QCOMPARE(tasks.port.calls.size(), 2);

  ActiveApplicationController blind(&taskList, {false, true});
  QVERIFY(!blind.available());
  QVERIFY(!blind.hasActiveWindow());
  QCOMPARE(blind.phaseText(), QStringLiteral("unavailable"));
  QVERIFY(!blind.close());
  QCOMPARE(tasks.port.calls.size(), 2);
}

QTEST_GUILESS_MAIN(QuickLaunchAndActiveApplicationTests)
#include "tst_quick_launch_and_active_application.moc"
