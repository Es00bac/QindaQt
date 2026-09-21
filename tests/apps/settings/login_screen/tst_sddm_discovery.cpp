// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_login_screen/sddm_discovery.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::SettingsLoginScreen;

namespace {

void writeFile(const QString &path, const QString &contents) {
  QFile file(path);
  QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text),
           qPrintable(file.errorString()));
  file.write(contents.toUtf8());
}

void makeTheme(const QString &root, const QString &id, const QString &name,
               bool qindaqt, bool preview) {
  const QDir dir(root);
  QVERIFY(dir.mkpath(id));
  writeFile(dir.absoluteFilePath(id + QStringLiteral("/Main.qml")),
            QStringLiteral("// theme\n"));
  if (!name.isEmpty()) {
    writeFile(dir.absoluteFilePath(id + QStringLiteral("/metadata.desktop")),
              QStringLiteral("[SddmGreeterTheme]\nName=%1\n").arg(name));
  }
  if (qindaqt) {
    writeFile(dir.absoluteFilePath(id + QStringLiteral("/.qindaqt-sddm-theme")),
              QString());
  }
  if (preview) {
    writeFile(dir.absoluteFilePath(id + QStringLiteral("/preview.png")),
              QStringLiteral("png"));
  }
}

} // namespace

class SddmDiscoveryTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void themesListQindaqtFirstThenAlphabetical();
  void themeFallsBackToDirectoryNameAndSkipsNonThemes();
  void sessionsListWaylandFirstAndSkipUnusableEntries();
  void loginUsersAreFilteredByUidAndShell();
};

void SddmDiscoveryTest::themesListQindaqtFirstThenAlphabetical() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  makeTheme(root.path(), QStringLiteral("zeta-other"), QStringLiteral("Zeta"),
            false, false);
  makeTheme(root.path(), QStringLiteral("qinda-b"), QStringLiteral("Beta Qinda"),
            true, true);
  makeTheme(root.path(), QStringLiteral("alpha-other"), QStringLiteral("Alpha"),
            false, false);
  makeTheme(root.path(), QStringLiteral("qinda-a"), QStringLiteral("Aardvark Qinda"),
            true, false);

  const QList<SddmThemeEntry> themes = listSddmThemes(root.path());
  QCOMPARE(themes.size(), 4);
  // QindaQt themes first, alphabetical inside the group, then the rest.
  QCOMPARE(themes.at(0).id, QStringLiteral("qinda-a"));
  QCOMPARE(themes.at(1).id, QStringLiteral("qinda-b"));
  QCOMPARE(themes.at(2).id, QStringLiteral("alpha-other"));
  QCOMPARE(themes.at(3).id, QStringLiteral("zeta-other"));
  QVERIFY(themes.at(0).qindaqt);
  QVERIFY(!themes.at(2).qindaqt);
  QVERIFY(themes.at(1).previewPath.endsWith(QStringLiteral("preview.png")));
  QCOMPARE(themes.at(0).previewPath, QString());
}

void SddmDiscoveryTest::themeFallsBackToDirectoryNameAndSkipsNonThemes() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  makeTheme(root.path(), QStringLiteral("bare"), QString(), false, false);
  // A directory with files but no Main.qml is not a loadable theme.
  QVERIFY(QDir(root.path()).mkpath(QStringLiteral("not-a-theme")));
  writeFile(root.filePath(QStringLiteral("not-a-theme/metadata.desktop")),
            QStringLiteral("[SddmGreeterTheme]\nName=Ghost\n"));

  const QList<SddmThemeEntry> themes = listSddmThemes(root.path());
  QCOMPARE(themes.size(), 1);
  QCOMPARE(themes.at(0).id, QStringLiteral("bare"));
  QCOMPARE(themes.at(0).name, QStringLiteral("bare"));
}

void SddmDiscoveryTest::sessionsListWaylandFirstAndSkipUnusableEntries() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString wayland = root.filePath(QStringLiteral("wayland-sessions"));
  const QString x11 = root.filePath(QStringLiteral("xsessions"));
  QVERIFY(QDir().mkpath(wayland));
  QVERIFY(QDir().mkpath(x11));
  writeFile(QDir(wayland).absoluteFilePath(QStringLiteral("zult.desktop")),
            QStringLiteral("[Desktop Entry]\nName=Zult\nExec=zult\n"));
  writeFile(QDir(wayland).absoluteFilePath(QStringLiteral("alpha.desktop")),
            QStringLiteral("[Desktop Entry]\nName=Alpha\nExec=alpha\n"
                           "Comment=The alpha session\n"));
  writeFile(QDir(wayland).absoluteFilePath(QStringLiteral("hidden.desktop")),
            QStringLiteral("[Desktop Entry]\nName=Hidden\nExec=x\nHidden=true\n"));
  writeFile(QDir(wayland).absoluteFilePath(QStringLiteral("nameless.desktop")),
            QStringLiteral("[Desktop Entry]\nExec=x\n"));
  writeFile(QDir(wayland).absoluteFilePath(QStringLiteral("noexec.desktop")),
            QStringLiteral("[Desktop Entry]\nName=NoExec\n"));
  writeFile(QDir(x11).absoluteFilePath(QStringLiteral("xone.desktop")),
            QStringLiteral("[Desktop Entry]\nName=X One\nExec=xone\n"));

  const QList<SddmSessionEntry> sessions =
      listSddmSessions({wayland}, {x11});
  QCOMPARE(sessions.size(), 3);
  // Alphabetical within the wayland group, X11 sessions after.
  QCOMPARE(sessions.at(0).id, QStringLiteral("alpha.desktop"));
  QCOMPARE(sessions.at(0).name, QStringLiteral("Alpha"));
  QCOMPARE(sessions.at(0).comment, QStringLiteral("The alpha session"));
  QVERIFY(sessions.at(0).wayland);
  QCOMPARE(sessions.at(1).id, QStringLiteral("zult.desktop"));
  QCOMPARE(sessions.at(2).id, QStringLiteral("xone.desktop"));
  QVERIFY(!sessions.at(2).wayland);
}

void SddmDiscoveryTest::loginUsersAreFilteredByUidAndShell() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString passwd = root.filePath(QStringLiteral("passwd"));
  writeFile(passwd, QStringLiteral(
      "root:x:0:0:root:/root:/bin/bash\n"
      "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin\n"
      "ada:x:1000:1000:Ada:/home/ada:/bin/bash\n"
      "bob:x:1001:1001:Bob:/home/bob:/bin/false\n"
      "cid:x:1002:1002:Cid:/home/cid:/usr/sbin/nologin\n"
      "dee:x:1003:1003:Dee:/home/dee:/bin/zsh\n"
      "service:x:999:999:Service:/var/lib/service:/bin/bash\n"));

  const QStringList users = listSddmLoginUsers(passwd);
  QCOMPARE(users, QStringList({QStringLiteral("ada"), QStringLiteral("dee")}));
}

QTEST_MAIN(SddmDiscoveryTest)
#include "tst_sddm_discovery.moc"
