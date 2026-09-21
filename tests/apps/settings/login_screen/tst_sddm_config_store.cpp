// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_login_screen/sddm_config_store.h>

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

} // namespace

class SddmConfigStoreTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void laterFilesWinAcrossDirectoriesAndTheLegacyFile();
  void winnerFileIsReportedPerKey();
  void keysSetOnlyInLaterFilesShadowTheOwnedDropIn();
  void unreadableFilesAreReportedNotHidden();
};

void SddmConfigStoreTest::laterFilesWinAcrossDirectoriesAndTheLegacyFile() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString usrLib = root.filePath(QStringLiteral("usr-lib"));
  const QString etcD = root.filePath(QStringLiteral("etc-d"));
  QVERIFY(QDir().mkpath(usrLib));
  QVERIFY(QDir().mkpath(etcD));
  writeFile(QDir(usrLib).absoluteFilePath(QStringLiteral("defaults.conf")),
            QStringLiteral("[Theme]\nCurrent=distro\n[General]\nNumlock=off\n"));
  writeFile(QDir(etcD).absoluteFilePath(QStringLiteral("01gentoo.conf")),
            QStringLiteral("[General]\nNumlock=none\n"));
  writeFile(QDir(etcD).absoluteFilePath(QStringLiteral("zz-theme.conf")),
            QStringLiteral("[Theme]\nCurrent=qinda-reclaimed\n"));
  const QString legacy = root.filePath(QStringLiteral("sddm.conf"));
  writeFile(legacy, QStringLiteral("[General]\nNumlock=on\n"));

  const SddmConfigReadResult read =
      readSddmConfig({usrLib, etcD}, legacy);
  // Later directory beats earlier, later file inside a directory beats
  // earlier, and the legacy main file beats every drop-in.
  QCOMPARE(read.values.theme, QStringLiteral("qinda-reclaimed"));
  QCOMPARE(read.values.numlock, QStringLiteral("on"));
  QCOMPARE(read.values.cursorTheme, QString());
  QCOMPARE(read.values.autologinUser, QString());
}

void SddmConfigStoreTest::winnerFileIsReportedPerKey() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString etcD = root.filePath(QStringLiteral("etc-d"));
  QVERIFY(QDir().mkpath(etcD));
  const QString a = QDir(etcD).absoluteFilePath(QStringLiteral("01a.conf"));
  const QString b = QDir(etcD).absoluteFilePath(QStringLiteral("02b.conf"));
  writeFile(a, QStringLiteral("[Theme]\nCurrent=first\n"));
  writeFile(b, QStringLiteral("[Theme]\nCursorTheme=whiteglass\n"));

  const SddmConfigReadResult read = readSddmConfig({etcD}, QString());
  QCOMPARE(read.winnerFileByKey.value(QStringLiteral("theme")), a);
  QCOMPARE(read.winnerFileByKey.value(QStringLiteral("cursorTheme")), b);
  QVERIFY(!read.winnerFileByKey.contains(QStringLiteral("numlock")));
}

void SddmConfigStoreTest::keysSetOnlyInLaterFilesShadowTheOwnedDropIn() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString etcD = root.filePath(QStringLiteral("etc-d"));
  QVERIFY(QDir().mkpath(etcD));
  const QString owned =
      QDir(etcD).absoluteFilePath(QStringLiteral("zzz-qindaqt-settings.conf"));
  writeFile(owned, QStringLiteral(
      "[Theme]\nCurrent=qinda-prism\n[General]\nNumlock=on\n"));
  const QString later =
      QDir(etcD).absoluteFilePath(QStringLiteral("zzzz-operator.conf"));
  writeFile(later, QStringLiteral("[Theme]\nCurrent=operator-choice\n"));

  const SddmConfigReadResult read = readSddmConfig({etcD}, QString(), owned);
  // The later file wins the value...
  QCOMPARE(read.values.theme, QStringLiteral("operator-choice"));
  QCOMPARE(read.values.numlock, QStringLiteral("on"));
  // ...and the shadowing is reported for exactly the shadowed key.
  QCOMPARE(read.shadowedKeys, QStringList({QStringLiteral("theme")}));
  QCOMPARE(read.shadowingFiles, QStringList({later}));

  // Without a later file touching the same keys, nothing is shadowed.
  const SddmConfigReadResult clean =
      readSddmConfig({etcD}, QString(),
                     QDir(etcD).absoluteFilePath(QStringLiteral("absent.conf")));
  QCOMPARE(clean.shadowedKeys, QStringList());
}

void SddmConfigStoreTest::unreadableFilesAreReportedNotHidden() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString etcD = root.filePath(QStringLiteral("etc-d"));
  QVERIFY(QDir().mkpath(etcD));
  const QString sealed = QDir(etcD).absoluteFilePath(QStringLiteral("sealed.conf"));
  writeFile(sealed, QStringLiteral("[Theme]\nCurrent=hidden-value\n"));
  QFile::setPermissions(sealed, QFileDevice::Permissions(0));

  const SddmConfigReadResult read = readSddmConfig({etcD}, QString());
  if (read.values.theme == QLatin1String("hidden-value")) {
    QSKIP("test is running with privileges that bypass file permissions");
  }
  QCOMPARE(read.unreadableFiles, QStringList({sealed}));
}

QTEST_MAIN(SddmConfigStoreTest)
#include "tst_sddm_config_store.moc"
