// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/open_with_controller.h"
#include "model/open_with_launcher.h"

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;
using QindaQt::ApplicationCatalog::DirectoryScan;
using QindaQt::ApplicationCatalog::ScannedApplication;
using QindaQt::Apps::SettingsDefaultApps::MimeAppsDefaultApplicationsStore;

// ADR-0269: Open With's bounded launch (widening ADR-0029) and its use of
// Settings' own association store. Nothing is ever spawned: every start is
// recorded by a fake starter.

namespace {

struct Started final {
  QString program;
  QStringList arguments;
};

struct StartRecorder final {
  QVector<Started> started;
  bool accept = true;

  [[nodiscard]] DetachedStarter starter() {
    return [this](const QString &program, const QStringList &arguments) {
      started.append({program, arguments});
      return accept;
    };
  }
};

[[nodiscard]] ScannedApplication application(const QString &id, const QString &body) {
  ScannedApplication scanned;
  scanned.entry.id = id;
  scanned.entry.name = id;
  scanned.desktopFilePath = QStringLiteral("/fixture/applications/%1.desktop").arg(id);
  scanned.documentText =
      QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\n").arg(id) + body;
  return scanned;
}

// A readable regular file; returns the absolute path the launcher hands over.
[[nodiscard]] QString makeFile(const QTemporaryDir &directory, const QString &name) {
  const QString path = directory.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write("fixture") != 7) {
    return {};
  }
  return QFileInfo(path).absoluteFilePath();
}

[[nodiscard]] DirectoryScan handlerScan() {
  DirectoryScan scan;
  scan.applications = {
      application(QStringLiteral("editor"), QStringLiteral("Exec=editor %F\nMimeType=text/plain;\n")),
      application(QStringLiteral("ide"), QStringLiteral("Exec=ide %F\nMimeType=text/x-csrc;\n")),
      application(QStringLiteral("viewer"), QStringLiteral("Exec=viewer %F\nMimeType=image/png;\n")),
      application(QStringLiteral("universal"),
                  QStringLiteral("Exec=universal %F\nMimeType=text/plain;image/png;\n"))};
  return scan;
}

[[nodiscard]] QStringList idsOf(const QVariantList &candidates) {
  QStringList ids;
  for (const QVariant &candidate : candidates) {
    ids.append(candidate.toMap().value(QStringLiteral("id")).toString());
  }
  return ids;
}

// The production OpenWithController over a fixed catalog scan, a real store
// on a temporary mimeapps.list and a recording starter.
struct ControllerFixture final {
  explicit ControllerFixture(const QString &mimeappsPath, ListingResult rows = {})
      : controller([] { return handlerScan(); }, [rows] { return rows; },
                   std::make_unique<MimeAppsDefaultApplicationsStore>(
                       mimeappsPath, QStringList{mimeappsPath}, DirectoryScan{}),
                   OpenWithLauncher(recorder.starter(), OpenWithLauncher::desktopTerminalPrefix())) {}

  StartRecorder recorder;
  OpenWithController controller;
};

} // namespace

class OpenWithTest final : public QObject {
  Q_OBJECT

private slots:
  void listExecOpensEveryFileInOneStart();
  void singleFileExecStartsOncePerFile();
  void applicationsThatOpenNoFilesAreRefused();
  void terminalEntriesRunInsideTheDesktopTerminal();
  void refusesWhatItCannotStartSafely();
  void linksAreResolvedBeforeHandingOver();
  void candidatesFollowTheTypeThenItsParents();
  void theDefaultIsListedFirstAndMarked();
  void severalFilesShareOnlyTheirCommonHandlers();
  void foldersAndRemoteAddressesHaveNoCandidates();
  void alwaysOpenWithWritesTheSharedStore();
  void alwaysOpenWithReportsARefusal();
  void openWithStartsTheChosenApplication();
  void otherApplicationListsThePlaceRowsAToZ();
};

void OpenWithTest::listExecOpensEveryFileInOneStart() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString first = makeFile(directory, QStringLiteral("a b.txt"));
  const QString second = makeFile(directory, QStringLiteral("c;d.txt"));
  StartRecorder recorder;
  const OpenWithLauncher launcher(recorder.starter(), OpenWithLauncher::desktopTerminalPrefix());
  const LaunchResult result = launcher.launch(
      application(QStringLiteral("viewer"), QStringLiteral("Exec=viewer --new %U\n")),
      {first, second});
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(recorder.started.size(), 1);
  QCOMPARE(recorder.started.first().program, QStringLiteral("viewer"));
  // Local paths, each one whole argument: no URL, no shell.
  QCOMPARE(recorder.started.first().arguments,
           QStringList({QStringLiteral("--new"), first, second}));
}

void OpenWithTest::singleFileExecStartsOncePerFile() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString first = makeFile(directory, QStringLiteral("one.txt"));
  const QString second = makeFile(directory, QStringLiteral("two.txt"));
  StartRecorder recorder;
  const OpenWithLauncher launcher(recorder.starter(), OpenWithLauncher::desktopTerminalPrefix());
  const LaunchResult result = launcher.launch(
      application(QStringLiteral("editor"), QStringLiteral("Exec=editor --line %f\n")),
      {first, second});
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(recorder.started.size(), 2);
  QCOMPARE(recorder.started.at(0).arguments, QStringList({QStringLiteral("--line"), first}));
  QCOMPARE(recorder.started.at(1).arguments, QStringList({QStringLiteral("--line"), second}));
}

void OpenWithTest::applicationsThatOpenNoFilesAreRefused() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString file = makeFile(directory, QStringLiteral("notes.txt"));
  StartRecorder recorder;
  const OpenWithLauncher launcher(recorder.starter(), OpenWithLauncher::desktopTerminalPrefix());
  const LaunchResult result = launcher.launch(
      application(QStringLiteral("settings"), QStringLiteral("Exec=settings --page\n")), {file});
  QCOMPARE(result.error, LaunchError::LaunchFailed);
  QVERIFY(!result.diagnostic.isEmpty());
  QVERIFY(recorder.started.isEmpty());
}

void OpenWithTest::terminalEntriesRunInsideTheDesktopTerminal() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString file = makeFile(directory, QStringLiteral("notes.txt"));
  StartRecorder recorder;
  const OpenWithLauncher launcher(recorder.starter(), OpenWithLauncher::desktopTerminalPrefix());
  const LaunchResult result = launcher.launch(
      application(QStringLiteral("vim"), QStringLiteral("Exec=vim %F\nTerminal=true\n")), {file});
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(recorder.started.size(), 1);
  QCOMPARE(recorder.started.first().program, QStringLiteral("qqterm"));
  QCOMPARE(recorder.started.first().arguments,
           QStringList({QStringLiteral("-e"), QStringLiteral("vim"), file}));

  // Without a terminal prefix a terminal entry is refused, never run bare.
  const OpenWithLauncher bare(recorder.starter(), {});
  QCOMPARE(bare.launch(application(QStringLiteral("vim"),
                                   QStringLiteral("Exec=vim %F\nTerminal=true\n")),
                       {file}).error,
           LaunchError::LaunchFailed);
  QCOMPARE(recorder.started.size(), 1);
}

void OpenWithTest::refusesWhatItCannotStartSafely() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString file = makeFile(directory, QStringLiteral("notes.txt"));
  QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("folder")));
  StartRecorder recorder;
  const OpenWithLauncher launcher(recorder.starter(), OpenWithLauncher::desktopTerminalPrefix());
  const ScannedApplication editor =
      application(QStringLiteral("editor"), QStringLiteral("Exec=editor %F\n"));

  QCOMPARE(launcher.launch(application(QStringLiteral("bus"),
                                       QStringLiteral("Exec=bus %F\nDBusActivatable=true\n")),
                           {file}).error,
           LaunchError::LaunchFailed);
  QCOMPARE(launcher.launch(editor, {directory.filePath(QStringLiteral("missing.txt"))}).error,
           LaunchError::NotFound);
  QCOMPARE(launcher.launch(editor, {directory.filePath(QStringLiteral("folder"))}).error,
           LaunchError::NotRegularFile);
  QCOMPARE(launcher.launch(editor, {}).error, LaunchError::LaunchFailed);
  QStringList tooMany;
  for (int index = 0; index <= OpenWithLauncher::maximumFiles; ++index) {
    tooMany.append(file);
  }
  QCOMPARE(launcher.launch(editor, tooMany).error, LaunchError::LaunchFailed);
  QVERIFY(recorder.started.isEmpty());

  // A start the system refuses is reported, not assumed.
  recorder.accept = false;
  QCOMPARE(launcher.launch(editor, {file}).error, LaunchError::LaunchFailed);
}

void OpenWithTest::linksAreResolvedBeforeHandingOver() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString target = makeFile(directory, QStringLiteral("target.txt"));
  const QString link = directory.filePath(QStringLiteral("link.txt"));
  QVERIFY(QFile::link(target, link));
  StartRecorder recorder;
  const OpenWithLauncher launcher(recorder.starter(), OpenWithLauncher::desktopTerminalPrefix());
  const LaunchResult result = launcher.launch(
      application(QStringLiteral("editor"), QStringLiteral("Exec=editor %F\n")), {link});
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(recorder.started.first().arguments, QStringList{QFileInfo(target).canonicalFilePath()});
}

void OpenWithTest::candidatesFollowTheTypeThenItsParents() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString source = makeFile(directory, QStringLiteral("main.c"));
  ControllerFixture fixture(directory.filePath(QStringLiteral("mimeapps.list")));
  QCOMPARE(fixture.controller.mimeTypeFor({source}), QStringLiteral("text/x-csrc"));
  // text/x-csrc's own handler first, then text/plain's (its parent type).
  const QStringList ids = idsOf(fixture.controller.candidatesFor({source}));
  QVERIFY2(!ids.isEmpty() && ids.first() == QStringLiteral("ide.desktop"), qPrintable(ids.join(u',')));
  QVERIFY(ids.contains(QStringLiteral("editor.desktop")));
  QVERIFY(ids.contains(QStringLiteral("universal.desktop")));
  QVERIFY(!ids.contains(QStringLiteral("viewer.desktop")));
  for (const QVariant &candidate : fixture.controller.candidatesFor({source})) {
    QVERIFY(!candidate.toMap().value(QStringLiteral("isDefault")).toBool());
  }
}

void OpenWithTest::theDefaultIsListedFirstAndMarked() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString notes = makeFile(directory, QStringLiteral("notes.txt"));
  const QString mimeapps = directory.filePath(QStringLiteral("mimeapps.list"));
  QFile list(mimeapps);
  QVERIFY(list.open(QIODevice::WriteOnly));
  QVERIFY(list.write("[Default Applications]\ntext/plain=universal.desktop;\n") > 0);
  list.close();
  ControllerFixture fixture(mimeapps);
  const QVariantList candidates = fixture.controller.candidatesFor({notes});
  QCOMPARE(idsOf(candidates),
           QStringList({QStringLiteral("universal.desktop"), QStringLiteral("editor.desktop")}));
  QVERIFY(candidates.at(0).toMap().value(QStringLiteral("isDefault")).toBool());
  QVERIFY(!candidates.at(1).toMap().value(QStringLiteral("isDefault")).toBool());
  QCOMPARE(candidates.at(0).toMap().value(QStringLiteral("name")).toString(),
           QStringLiteral("universal"));
}

void OpenWithTest::severalFilesShareOnlyTheirCommonHandlers() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString notes = makeFile(directory, QStringLiteral("notes.txt"));
  const QString picture = makeFile(directory, QStringLiteral("picture.png"));
  ControllerFixture fixture(directory.filePath(QStringLiteral("mimeapps.list")));
  QCOMPARE(idsOf(fixture.controller.candidatesFor({notes, picture})),
           QStringList{QStringLiteral("universal.desktop")});
  // A mixed selection has no single type to make a default for.
  QVERIFY(fixture.controller.mimeTypeFor({notes, picture}).isEmpty());
  QCOMPARE(fixture.controller.mimeTypeFor({notes}), QStringLiteral("text/plain"));
  QVERIFY(!fixture.controller.mimeDescription(QStringLiteral("text/plain")).isEmpty());
}

void OpenWithTest::foldersAndRemoteAddressesHaveNoCandidates() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("folder")));
  ControllerFixture fixture(directory.filePath(QStringLiteral("mimeapps.list")));
  QVERIFY(fixture.controller.candidatesFor({directory.filePath(QStringLiteral("folder"))}).isEmpty());
  QVERIFY(fixture.controller.candidatesFor({QStringLiteral("sftp://host/notes.txt")}).isEmpty());
  QVERIFY(fixture.controller.candidatesFor({QStringLiteral("applications:editor")}).isEmpty());
  QVERIFY(fixture.controller.candidatesFor({}).isEmpty());
}

void OpenWithTest::alwaysOpenWithWritesTheSharedStore() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString notes = makeFile(directory, QStringLiteral("notes.txt"));
  const QString mimeapps = directory.filePath(QStringLiteral("mimeapps.list"));
  ControllerFixture fixture(mimeapps);
  // ide does not declare text/plain; the store also records the association.
  QVERIFY(fixture.controller.setDefault(QStringLiteral("text/plain"), QStringLiteral("ide.desktop")));
  QVERIFY(fixture.controller.lastError().isEmpty());
  QFile written(mimeapps);
  QVERIFY(written.open(QIODevice::ReadOnly));
  const QString contents = QString::fromUtf8(written.readAll());
  QVERIFY(contents.contains(QStringLiteral("[Default Applications]")));
  QVERIFY(contents.contains(QStringLiteral("text/plain=ide.desktop;")));
  QVERIFY(contents.contains(QStringLiteral("[Added Associations]")));
  const QVariantList candidates = fixture.controller.candidatesFor({notes});
  QCOMPARE(idsOf(candidates).first(), QStringLiteral("ide.desktop"));
  QVERIFY(candidates.first().toMap().value(QStringLiteral("isDefault")).toBool());
}

void OpenWithTest::alwaysOpenWithReportsARefusal() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString mimeapps = directory.filePath(QStringLiteral("mimeapps.list"));
  ControllerFixture fixture(mimeapps);
  QSignalSpy errors(&fixture.controller, &OpenWithController::lastErrorChanged);
  QVERIFY(!fixture.controller.setDefault(QStringLiteral("text/plain"),
                                         QStringLiteral("missing.desktop")));
  QVERIFY(!fixture.controller.lastError().isEmpty());
  QCOMPARE(errors.size(), 1);
  QVERIFY(!QFileInfo::exists(mimeapps));
  fixture.controller.clearLastError();
  QVERIFY(fixture.controller.lastError().isEmpty());
}

void OpenWithTest::openWithStartsTheChosenApplication() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString notes = makeFile(directory, QStringLiteral("notes.txt"));
  ControllerFixture fixture(directory.filePath(QStringLiteral("mimeapps.list")));
  QVERIFY(fixture.controller.openWith(QStringLiteral("editor.desktop"), {notes}));
  QCOMPARE(fixture.recorder.started.size(), 1);
  QCOMPARE(fixture.recorder.started.first().program, QStringLiteral("editor"));
  QCOMPARE(fixture.recorder.started.first().arguments, QStringList{notes});
  QVERIFY(!fixture.controller.openWith(QStringLiteral("missing.desktop"), {notes}));
  QVERIFY(!fixture.controller.lastError().isEmpty());
  QCOMPARE(fixture.recorder.started.size(), 1);
}

void OpenWithTest::otherApplicationListsThePlaceRowsAToZ() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  ListingResult rows;
  const auto row = [](const QString &name, const QString &id) {
    DirectoryEntry entry;
    entry.name = name;
    entry.applicationId = id;
    entry.iconName = id + QStringLiteral("-icon");
    return entry;
  };
  rows.entries = {row(QStringLiteral("Zeta Player"), QStringLiteral("zeta")),
                  row(QStringLiteral("alpha editor"), QStringLiteral("alpha")),
                  row(QStringLiteral("not an application"), QString())};
  ControllerFixture fixture(directory.filePath(QStringLiteral("mimeapps.list")), rows);
  const QVariantList all = fixture.controller.allApplications();
  QCOMPARE(idsOf(all), QStringList({QStringLiteral("alpha.desktop"), QStringLiteral("zeta.desktop")}));
  QCOMPARE(all.first().toMap().value(QStringLiteral("iconName")).toString(),
           QStringLiteral("alpha-icon"));
}

QTEST_GUILESS_MAIN(OpenWithTest)
#include "tst_open_with.moc"
