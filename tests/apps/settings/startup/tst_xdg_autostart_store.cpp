// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_startup/startup_settings_model.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>
#include <QTest>

using namespace QindaQt::Apps::SettingsStartup;

namespace {

void writeDesktopFile(const QString &path, const QString &contents) {
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  QTextStream stream(&file);
  stream << contents;
}

} // namespace

class XdgAutostartStoreTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void listsUserAndSystemEntriesMerged();
  void userEntryShadowsSystemEntryOfTheSameId();
  void disablingASystemEntryWritesAUserOverrideWithoutTouchingTheSystemFile();
  void enablingASystemEntryAfterOverrideRestoresIt();
  void enablingClearsEveryRecognizedDisableFlag();
  void rejectsUnsafeIds();
  void disablingAUserOnlyEntryPatchesItInPlace();
  void invalidFilesAreSkipped();
  void addCommandCreatesAMarkedCustomEntry();
  void addCommandDeduplicatesIds();
  void addCommandRejectsEmptyNameOrCommand();
  void removeCustomDeletesOnlyMarkedEntries();
  void removeCustomRefusesANonCustomEntry();
};

void XdgAutostartStoreTest::listsUserAndSystemEntriesMerged() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  const QString systemDir = root.filePath(QStringLiteral("system/autostart"));
  QVERIFY(QDir().mkpath(userDir));
  QVERIFY(QDir().mkpath(systemDir));

  writeDesktopFile(
      QDir(userDir).filePath(QStringLiteral("alpha.desktop")),
      QStringLiteral("[Desktop Entry]\nType=Application\nName=Alpha\nExec=alpha\n"));
  writeDesktopFile(
      QDir(systemDir).filePath(QStringLiteral("beta.desktop")),
      QStringLiteral("[Desktop Entry]\nType=Application\nName=Beta\nExec=beta\n"));

  XdgAutostartStore store(userDir, {systemDir});
  QString error;
  const QList<AutostartEntry> entries = store.list(&error);
  QCOMPARE(entries.size(), 2);
  QCOMPARE(entries.at(0).name, QStringLiteral("Alpha"));
  QCOMPARE(entries.at(1).name, QStringLiteral("Beta"));
  QVERIFY(entries.at(0).enabled);
  QVERIFY(entries.at(1).enabled);
  QVERIFY(!entries.at(0).custom);
}

void XdgAutostartStoreTest::userEntryShadowsSystemEntryOfTheSameId() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  const QString systemDir = root.filePath(QStringLiteral("system/autostart"));
  QVERIFY(QDir().mkpath(userDir));
  QVERIFY(QDir().mkpath(systemDir));

  writeDesktopFile(
      QDir(systemDir).filePath(QStringLiteral("shared.desktop")),
      QStringLiteral(
          "[Desktop Entry]\nType=Application\nName=System Name\nExec=x\n"));
  writeDesktopFile(
      QDir(userDir).filePath(QStringLiteral("shared.desktop")),
      QStringLiteral(
          "[Desktop Entry]\nType=Application\nName=User Name\nExec=x\nHidden=true\n"));

  XdgAutostartStore store(userDir, {systemDir});
  QString error;
  const QList<AutostartEntry> entries = store.list(&error);
  QCOMPARE(entries.size(), 1);
  QCOMPARE(entries.at(0).name, QStringLiteral("User Name"));
  QVERIFY(!entries.at(0).enabled);
}

void XdgAutostartStoreTest::
    disablingASystemEntryWritesAUserOverrideWithoutTouchingTheSystemFile() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  const QString systemDir = root.filePath(QStringLiteral("system/autostart"));
  QVERIFY(QDir().mkpath(systemDir));
  const QString systemPath =
      QDir(systemDir).filePath(QStringLiteral("app.desktop"));
  const QString systemContents = QStringLiteral(
      "[Desktop Entry]\nType=Application\nName=App\nExec=app\n"
      "X-Custom-Unrelated-Key=keep-me\n");
  writeDesktopFile(systemPath, systemContents);

  XdgAutostartStore store(userDir, {systemDir});
  QString error;
  QVERIFY(store.setEnabled(QStringLiteral("app"), false, &error));
  QCOMPARE(error, QString());

  // The system file is untouched.
  QFile systemFile(systemPath);
  QVERIFY(systemFile.open(QIODevice::ReadOnly | QIODevice::Text));
  QCOMPARE(QString::fromUtf8(systemFile.readAll()), systemContents);

  // The user override exists, preserves the unrelated key, and disables.
  const QString overridePath =
      QDir(userDir).filePath(QStringLiteral("app.desktop"));
  QVERIFY(QFile::exists(overridePath));
  QFile overrideFile(overridePath);
  QVERIFY(overrideFile.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString overrideContents = QString::fromUtf8(overrideFile.readAll());
  QVERIFY(overrideContents.contains(QStringLiteral("Hidden=true")));
  QVERIFY(overrideContents.contains(
      QStringLiteral("X-Custom-Unrelated-Key=keep-me")));

  const QList<AutostartEntry> entries = store.list(&error);
  QCOMPARE(entries.size(), 1);
  QVERIFY(!entries.at(0).enabled);
}

void XdgAutostartStoreTest::enablingASystemEntryAfterOverrideRestoresIt() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  const QString systemDir = root.filePath(QStringLiteral("system/autostart"));
  QVERIFY(QDir().mkpath(systemDir));
  writeDesktopFile(
      QDir(systemDir).filePath(QStringLiteral("app.desktop")),
      QStringLiteral("[Desktop Entry]\nType=Application\nName=App\nExec=app\n"));

  XdgAutostartStore store(userDir, {systemDir});
  QString error;
  QVERIFY(store.setEnabled(QStringLiteral("app"), false, &error));
  QVERIFY(!store.list(&error).at(0).enabled);

  QVERIFY(store.setEnabled(QStringLiteral("app"), true, &error));
  const QList<AutostartEntry> entries = store.list(&error);
  QCOMPARE(entries.size(), 1);
  QVERIFY(entries.at(0).enabled);
  // The override file exists (Hidden=false), not deleted; still shadows the
  // system copy but agrees with it.
  QVERIFY(QFile::exists(QDir(userDir).filePath(QStringLiteral("app.desktop"))));
}

void XdgAutostartStoreTest::enablingClearsEveryRecognizedDisableFlag() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  QVERIFY(QDir().mkpath(userDir));
  const QString path = QDir(userDir).filePath(QStringLiteral("app.desktop"));
  writeDesktopFile(path, QStringLiteral(
      "[Desktop Entry]\nType=Application\nName=App\nExec=app\n"
      "Hidden=true\nX-GNOME-Autostart-enabled=false\n"
      "X-GNOME-Autostart-enabled=false\nX-Unrelated=preserve\n"));
  XdgAutostartStore store(userDir, {});
  QString error;
  QVERIFY(store.setEnabled(QStringLiteral("app"), true, &error));
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString contents = QString::fromUtf8(file.readAll());
  QVERIFY(contents.contains(QStringLiteral("Hidden=false")));
  QCOMPARE(contents.count(QStringLiteral("X-GNOME-Autostart-enabled=true")), 2);
  QVERIFY(contents.contains(QStringLiteral("X-Unrelated=preserve")));
  QVERIFY(store.list(&error).constFirst().enabled);
}

void XdgAutostartStoreTest::rejectsUnsafeIds() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  XdgAutostartStore store(root.filePath(QStringLiteral("user/autostart")), {});
  QString error;
  QVERIFY(!store.setEnabled(QStringLiteral("../elsewhere"), true, &error));
  QVERIFY(!error.isEmpty());
  error.clear();
  QVERIFY(!store.removeCustom(QStringLiteral("../elsewhere"), &error));
  QVERIFY(!error.isEmpty());
}

void XdgAutostartStoreTest::disablingAUserOnlyEntryPatchesItInPlace() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  QVERIFY(QDir().mkpath(userDir));
  writeDesktopFile(
      QDir(userDir).filePath(QStringLiteral("mine.desktop")),
      QStringLiteral("[Desktop Entry]\nType=Application\nName=Mine\nExec=mine\n"));

  XdgAutostartStore store(userDir, {});
  QString error;
  QVERIFY(store.setEnabled(QStringLiteral("mine"), false, &error));
  const QList<AutostartEntry> entries = store.list(&error);
  QCOMPARE(entries.size(), 1);
  QVERIFY(!entries.at(0).enabled);
}

void XdgAutostartStoreTest::invalidFilesAreSkipped() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  QVERIFY(QDir().mkpath(userDir));
  // No [Desktop Entry] group at all.
  writeDesktopFile(QDir(userDir).filePath(QStringLiteral("junk.desktop")),
                   QStringLiteral("not a desktop file\n"));
  // Never declared Type=Application.
  writeDesktopFile(
      QDir(userDir).filePath(QStringLiteral("notype.desktop")),
      QStringLiteral("[Desktop Entry]\nName=No Type\nExec=x\n"));

  XdgAutostartStore store(userDir, {});
  QString error;
  QCOMPARE(store.list(&error).size(), 0);
}

void XdgAutostartStoreTest::addCommandCreatesAMarkedCustomEntry() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));

  XdgAutostartStore store(userDir, {});
  QString error;
  const QString id =
      store.addCommand(QStringLiteral("My Sync"), QStringLiteral("my-sync --daemon"),
                       &error);
  QVERIFY(!id.isEmpty());
  QCOMPARE(error, QString());

  const QList<AutostartEntry> entries = store.list(&error);
  QCOMPARE(entries.size(), 1);
  QCOMPARE(entries.at(0).name, QStringLiteral("My Sync"));
  QCOMPARE(entries.at(0).exec, QStringLiteral("my-sync --daemon"));
  QVERIFY(entries.at(0).enabled);
  QVERIFY(entries.at(0).custom);
}

void XdgAutostartStoreTest::addCommandDeduplicatesIds() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));

  XdgAutostartStore store(userDir, {});
  QString error;
  const QString first =
      store.addCommand(QStringLiteral("Sync"), QStringLiteral("cmd1"), &error);
  const QString second =
      store.addCommand(QStringLiteral("Sync"), QStringLiteral("cmd2"), &error);
  QVERIFY(!first.isEmpty());
  QVERIFY(!second.isEmpty());
  QVERIFY(first != second);
  QCOMPARE(store.list(&error).size(), 2);
}

void XdgAutostartStoreTest::addCommandRejectsEmptyNameOrCommand() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  XdgAutostartStore store(userDir, {});
  QString error;
  QVERIFY(store.addCommand(QStringLiteral("  "), QStringLiteral("cmd"), &error)
              .isEmpty());
  QVERIFY(!error.isEmpty());
  error.clear();
  QVERIFY(store.addCommand(QStringLiteral("Name"), QStringLiteral("  "), &error)
              .isEmpty());
  QVERIFY(!error.isEmpty());
}

void XdgAutostartStoreTest::removeCustomDeletesOnlyMarkedEntries() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  XdgAutostartStore store(userDir, {});
  QString error;
  const QString id =
      store.addCommand(QStringLiteral("Sync"), QStringLiteral("cmd"), &error);
  QVERIFY(!id.isEmpty());
  QVERIFY(store.removeCustom(id, &error));
  QCOMPARE(store.list(&error).size(), 0);
}

void XdgAutostartStoreTest::removeCustomRefusesANonCustomEntry() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString userDir = root.filePath(QStringLiteral("user/autostart"));
  QVERIFY(QDir().mkpath(userDir));
  writeDesktopFile(
      QDir(userDir).filePath(QStringLiteral("real-app.desktop")),
      QStringLiteral(
          "[Desktop Entry]\nType=Application\nName=Real App\nExec=real\n"));

  XdgAutostartStore store(userDir, {});
  QString error;
  QVERIFY(!store.removeCustom(QStringLiteral("real-app"), &error));
  QVERIFY(!error.isEmpty());
  QCOMPARE(store.list(&error).size(), 1);
}

QTEST_MAIN(XdgAutostartStoreTest)
#include "tst_xdg_autostart_store.moc"
