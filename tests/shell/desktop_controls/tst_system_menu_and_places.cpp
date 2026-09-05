// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_controls_test_support.h"

#include "qindaqt/shell/desktop_controls/file_manager_folder_opener.h"
#include "qindaqt/shell/desktop_controls/places_controller.h"
#include "qindaqt/shell/desktop_controls/system_menu_controller.h"

#include <QFile>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;

class SystemMenuAndPlacesTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void systemMenuOpensSettingsThroughTheLauncherFacade();
  void systemMenuFailsClosedWithoutGrantOrLauncher();
  void placesOpenOnlyKnownEntriesThroughTheSeam();
  void fileManagerOpenerSpawnsAbsoluteCandidatesOnly();
};

void SystemMenuAndPlacesTests::systemMenuOpensSettingsThroughTheLauncherFacade()
{
  LauncherStack stack;
  QVERIFY(stack.root.isValid());
  QVERIFY(stack.addEntry(QStringLiteral("org.qindaqt.Settings.desktop"),
                         QStringLiteral("System Settings"), QStringLiteral("/bin/true")));
  QVERIFY(stack.scanner.start());
  QindaQt::Shell::Launcher::LauncherAppletController launcher(&stack.scanner, nullptr,
                                                              &stack.executor, true);
  StubSessionActions session;
  SystemMenuController menu(&session, &launcher, true,
                            SystemMenuController::defaultSettingsEntryId(),
                            QStringLiteral("0.1.0"));
  QCOMPARE(menu.sessionActions(), &session);
  QVERIFY(menu.sessionActionsAvailable());
  QVERIFY(menu.canOpenSettings());
  QCOMPARE(menu.versionText(), QStringLiteral("0.1.0"));
  QCOMPARE(menu.productName(), QStringLiteral("QindaQt"));

  QVERIFY(menu.openSettings());
  QCOMPARE(stack.spawner.requests.size(), 1);
  QCOMPARE(stack.spawner.requests.constFirst().program, QStringLiteral("/bin/true"));
  QVERIFY(!menu.feedbackPresent());

  // A missing entry is a truthful refusal, never a second execution path.
  SystemMenuController missing(&session, &launcher, true, QStringLiteral("org.example.Absent"),
                               QStringLiteral("0.1.0"));
  QSignalSpy feedbackSpy(&missing, &SystemMenuController::feedbackChanged);
  QVERIFY(!missing.openSettings());
  QCOMPARE(stack.spawner.requests.size(), 1);
  QVERIFY(missing.feedbackPresent());
  QVERIFY(missing.feedback().startsWith(QStringLiteral("Could not open System Settings")));
  QCOMPARE(feedbackSpy.size(), 1);
  missing.clearFeedback();
  QVERIFY(!missing.feedbackPresent());
}

void SystemMenuAndPlacesTests::systemMenuFailsClosedWithoutGrantOrLauncher()
{
  LauncherStack stack;
  QVERIFY(stack.scanner.start());
  QindaQt::Shell::Launcher::LauncherAppletController launcher(&stack.scanner, nullptr,
                                                              &stack.executor, true);
  SystemMenuController ungranted(nullptr, &launcher, false,
                                 SystemMenuController::defaultSettingsEntryId(), {});
  QVERIFY(!ungranted.sessionActionsAvailable());
  QCOMPARE(ungranted.sessionActions(), nullptr);
  QVERIFY(!ungranted.canOpenSettings());
  QVERIFY(!ungranted.openSettings());
  QVERIFY(ungranted.feedback().contains(QStringLiteral("not granted")));
  QVERIFY(stack.spawner.requests.isEmpty());

  SystemMenuController detached(nullptr, nullptr, true,
                                SystemMenuController::defaultSettingsEntryId(), {});
  QVERIFY(!detached.canOpenSettings());
  QVERIFY(!detached.openSettings());
  QVERIFY(detached.feedback().contains(QStringLiteral("unavailable")));
}

void SystemMenuAndPlacesTests::placesOpenOnlyKnownEntriesThroughTheSeam()
{
  RecordingFolderOpener opener;
  const QList<PlaceEntry> entries{
      {QStringLiteral("home"), QStringLiteral("Home"), QStringLiteral("/home/fixture"),
       QStringLiteral("user-home")},
      {QStringLiteral("computer"), QStringLiteral("Computer"), QStringLiteral("/"),
       QStringLiteral("drive-harddisk")}};
  PlacesController places(&opener, true, entries);
  QVERIFY(places.available());
  QCOMPARE(places.count(), 2);
  const QVariantMap first = places.rows().constFirst().toMap();
  QCOMPARE(first.value(QStringLiteral("id")).toString(), QStringLiteral("home"));
  QCOMPARE(first.value(QStringLiteral("accessibleName")).toString(),
           QStringLiteral("Home, /home/fixture"));
  QCOMPARE(first.value(QStringLiteral("index")).toInt(), 0);

  QVERIFY(places.open(QStringLiteral("computer")));
  QCOMPARE(opener.opened, QStringList{QStringLiteral("/")});
  QVERIFY(!places.open(QStringLiteral("music")));
  QCOMPARE(opener.opened.size(), 1);
  QVERIFY(places.feedback().contains(QStringLiteral("no longer available")));

  opener.nextResult = {false, QStringLiteral("spawn refused")};
  QVERIFY(!places.open(QStringLiteral("home")));
  QCOMPARE(opener.opened.size(), 2);
  QCOMPARE(places.feedback(), QStringLiteral("Could not open Home: spawn refused"));

  PlacesController ungranted(&opener, false, entries);
  QVERIFY(!ungranted.available());
  QVERIFY(!ungranted.open(QStringLiteral("home")));
  QCOMPARE(opener.opened.size(), 2);

  PlacesController detached(nullptr, true, entries);
  QVERIFY(!detached.available());
  QVERIFY(!detached.open(QStringLiteral("home")));
  QVERIFY(detached.feedback().contains(QStringLiteral("unavailable")));

  QList<PlaceEntry> tooMany;
  for (int index = 0; index < PlacesController::MaxPlaces + 5; ++index) {
    tooMany.append({QStringLiteral("p%1").arg(index), QStringLiteral("P%1").arg(index),
                    QStringLiteral("/p%1").arg(index), QStringLiteral("folder")});
  }
  PlacesController bounded(&opener, true, tooMany);
  QCOMPARE(bounded.count(), PlacesController::MaxPlaces);
}

void SystemMenuAndPlacesTests::fileManagerOpenerSpawnsAbsoluteCandidatesOnly()
{
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString folder = root.filePath(QStringLiteral("Documents"));
  QVERIFY(QDir().mkpath(folder));
  const QString program = root.filePath(QStringLiteral("qindaqt-file-manager"));
  {
    QFile file(program);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("#!/bin/sh\nexit 0\n") > 0);
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  }

  QindaQt::Tests::Launcher::RecordingSpawner spawner;
  FileManagerFolderOpener opener(spawner, {QStringLiteral("qindaqt-file-manager"),
                                           root.filePath(QStringLiteral("missing")),
                                           program});
  QCOMPARE(opener.resolvedProgram(), program);
  const FolderOpener::Result opened = opener.open(folder);
  QVERIFY2(opened.ok, qPrintable(opened.diagnostic));
  QCOMPARE(spawner.requests.size(), 1);
  QCOMPARE(spawner.requests.constFirst().program, program);
  QCOMPARE(spawner.requests.constFirst().arguments, QStringList{folder});

  const FolderOpener::Result notDirectory = opener.open(program);
  QVERIFY(!notDirectory.ok);
  QVERIFY(notDirectory.diagnostic.contains(QStringLiteral("directory")));
  const FolderOpener::Result relative = opener.open(QStringLiteral("Documents"));
  QVERIFY(!relative.ok);
  QCOMPARE(spawner.requests.size(), 1);

  FileManagerFolderOpener absent(spawner, {root.filePath(QStringLiteral("missing"))});
  QVERIFY(absent.resolvedProgram().isEmpty());
  const FolderOpener::Result noProgram = absent.open(folder);
  QVERIFY(!noProgram.ok);
  QVERIFY(noProgram.diagnostic.contains(QStringLiteral("not installed")));
  QCOMPARE(spawner.requests.size(), 1);

  spawner.nextResult = {false, QStringLiteral("start failed")};
  const FolderOpener::Result failed = opener.open(folder);
  QVERIFY(!failed.ok);
  QCOMPARE(failed.diagnostic, QStringLiteral("start failed"));
}

QTEST_GUILESS_MAIN(SystemMenuAndPlacesTests)
#include "tst_system_menu_and_places.moc"
