// SPDX-License-Identifier: GPL-3.0-or-later
#include "restore/restore_state_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::TextEditor;

class RestoreStateTest final : public QObject {
  Q_OBJECT

private slots:
  void roundTripContainsPathsOnly();
  void malformedInventoryIsRejectedWholesale();
  void oversizedAndSymlinkStateFailClosed();
};

void RestoreStateTest::roundTripContainsPathsOnly() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  RestoreStateStore store(directory.path());
  const RestoreState expected{
      .paths = {directory.filePath(QStringLiteral("a.txt")),
                directory.filePath(QStringLiteral("b.txt"))},
      .activeIndex = 1};
  QVERIFY(store.store(expected).ok());
  const QFileDevice::Permissions permissions =
      QFileInfo(store.filePath()).permissions();
  QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                           QFileDevice::ReadOther | QFileDevice::WriteOther)));
  const RestoreLoadResult loaded = store.load();
  QVERIFY(loaded.ok());
  QCOMPARE(*loaded.state, expected);

  QFile file(store.filePath());
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
  QCOMPARE(object.keys(),
           QStringList({QStringLiteral("activeIndex"), QStringLiteral("paths"),
                        QStringLiteral("version")}));
  QVERIFY(!object.contains(QStringLiteral("content")));
  QVERIFY(!object.contains(QStringLiteral("dirty")));
}

void RestoreStateTest::malformedInventoryIsRejectedWholesale() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  RestoreStateStore store(directory.path());
  QFile file(store.filePath());
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(
      file.write(R"({"version":1,"paths":["/one","/one"],"activeIndex":0})"),
      qint64(53));
  file.close();
  const RestoreLoadResult loaded = store.load();
  QVERIFY(!loaded.ok());
  QVERIFY(!loaded.state.has_value());
  QCOMPARE(loaded.error, RestoreStateError::Malformed);
}

void RestoreStateTest::oversizedAndSymlinkStateFailClosed() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  RestoreStateStore store(directory.filePath(QStringLiteral("state")));
  QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("state"))));
  QFile file(store.filePath());
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(QByteArray(RestoreStateStore::maximumBytes + 1, 'x')),
           RestoreStateStore::maximumBytes + 1);
  file.close();
  QCOMPARE(store.load().error, RestoreStateError::TooLarge);

  QVERIFY(QFile::remove(store.filePath()));
  const QString target = directory.filePath(QStringLiteral("target.json"));
  QFile targetFile(target);
  QVERIFY(targetFile.open(QIODevice::WriteOnly));
  targetFile.write("{}");
  targetFile.close();
  QVERIFY(QFile::link(target, store.filePath()));
  QCOMPARE(store.load().error, RestoreStateError::Malformed);
}

QTEST_GUILESS_MAIN(RestoreStateTest)
#include "tst_restore_state.moc"
