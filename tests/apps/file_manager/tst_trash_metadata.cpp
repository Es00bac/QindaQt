// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/trash_metadata.h"
#include <QTest>

using namespace QindaQt::Apps::FileManager;

class TrashMetadataTest final : public QObject {
  Q_OBJECT
private slots:
  void roundTrip_data() {
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("home");
    QTest::newRow("home") << QStringLiteral("/home/user/a") << true;
    QTest::newRow("volume") << QStringLiteral("/media/volume/folder/a") << false;
    QTest::newRow("controls-literal") << QStringLiteral("/media/volume/a\n%&<b>") << false;
    QTest::newRow("unicode") << QStringLiteral("/media/volume/\uFEFF\uFFFD\u732B") << false;
  }
  void roundTrip() {
    QFETCH(QString, path);
    QFETCH(bool, home);
    TrashLocation location{QStringLiteral("/home/user/data/Trash"), {}, true};
    if (!home) location = {QStringLiteral("/media/volume/.Trash-1000"),
                           QStringLiteral("/media/volume"), false};
    const auto date = QDateTime::fromString(QStringLiteral("2026-10-08T11:00:00"),
                                          QStringLiteral("yyyy-MM-dd'T'HH:mm:ss"));
    QByteArray encoded;
    QVERIFY(TrashMetadataCodec::encode(path, location, date, encoded).ok());
    if (!home) QVERIFY(!encoded.contains("Path=/"));
    TrashMetadata decoded;
    QVERIFY(TrashMetadataCodec::decode(encoded, location, decoded).ok());
    QCOMPARE(decoded.originalPath, path);
    QCOMPARE(decoded.deletionDate, date);
  }
  void refused_data() {
    QTest::addColumn<QByteArray>("path");
    QTest::newRow("traversal") << QByteArray("../outside");
    QTest::newRow("escaped-traversal") << QByteArray("%2E%2E/outside");
    QTest::newRow("internal-traversal") << QByteArray("a/../outside");
    QTest::newRow("absolute-volume") << QByteArray("/outside");
    QTest::newRow("nul") << QByteArray("a%00b");
    QTest::newRow("invalid-percent") << QByteArray("a%G0");
    QTest::newRow("short-percent") << QByteArray("a%");
    QTest::newRow("empty-component") << QByteArray("a//b");
    QTest::newRow("bad-native-byte") << QByteArray("%FF");
    QTest::newRow("empty-first") << QByteArray("\nPath=valid");
  }
  void refused() {
    QFETCH(QByteArray, path);
    TrashMetadata sentinel{QStringLiteral("untouched"), {}};
    const TrashLocation location{QStringLiteral("/volume/.Trash-1000"),
                                 QStringLiteral("/volume"), false};
    QVERIFY(!TrashMetadataCodec::decode(
        "[Trash Info]\nPath=" + path + "\nDeletionDate=2026-10-08T11:00:00\n",
        location, sentinel).ok());
    QCOMPARE(sentinel.originalPath, QStringLiteral("untouched"));
  }
  void firstOccurrenceAndUnknownKeys() {
    const TrashLocation location{QStringLiteral("/volume/.Trash-1000"),
                                 QStringLiteral("/volume"), false};
    TrashMetadata decoded;
    QVERIFY(TrashMetadataCodec::decode(
        "[Trash Info]\nX-Future=value\nPath=first%25name\nPath=ignored\n"
        "DeletionDate=2026-10-08T11:00:00\nDeletionDate=bad\n",
        location, decoded).ok());
    QCOMPARE(decoded.originalPath, QStringLiteral("/volume/first%name"));
  }
  void standardCrLfRecord() {
    const TrashLocation location{QStringLiteral("/volume/.Trash-1000"), QStringLiteral("/volume"), false};
    TrashMetadata decoded;
    QVERIFY(TrashMetadataCodec::decode("[Trash Info]\r\nPath=a%0Db\r\nDeletionDate=2026-10-08T11:00:00\r\n", location, decoded).ok());
    QCOMPARE(decoded.originalPath, QStringLiteral("/volume/a\rb"));
  }
  void homeRelativePathUsesDataHome() {
    const TrashLocation location{QStringLiteral("/home/user/data/Trash"), {}, true};
    TrashMetadata decoded;
    QVERIFY(TrashMetadataCodec::decode(
        "[Trash Info]\nPath=old/file\nDeletionDate=2026-10-08T11:00:00\n",
        location, decoded).ok());
    QCOMPARE(decoded.originalPath, QStringLiteral("/home/user/data/old/file"));
  }
  void invalidFirstDateAndHeaderRefuse() {
    const TrashLocation location{QStringLiteral("/volume/.Trash-1000"), QStringLiteral("/volume"), false};
    TrashMetadata sentinel{QStringLiteral("unchanged"), {}};
    for (const auto &bytes : {
        QByteArray("[Wrong]\nPath=file\nDeletionDate=2026-10-08T11:00:00\n"),
        QByteArray("[Trash Info]\nPath=file\nDeletionDate=\nDeletionDate=2026-10-08T11:00:00\n"),
        QByteArray("[Trash Info]\nPath=file\nDeletionDate=2026-02-30T11:00:00\n")}) {
      QVERIFY(!TrashMetadataCodec::decode(bytes, location, sentinel).ok());
      QCOMPARE(sentinel.originalPath, QStringLiteral("unchanged"));
    }
  }
  void boundsAndAtomicRefusal() {
    const TrashLocation location{QStringLiteral("/volume/.Trash-1000"),
                                 QStringLiteral("/volume"), false};
    TrashMetadata decoded{QStringLiteral("unchanged"), {}};
    QVERIFY(!TrashMetadataCodec::decode(QByteArray(4097, 'a'), location, decoded).ok());
    QCOMPARE(decoded.originalPath, QStringLiteral("unchanged"));
    QByteArray destination("unchanged");
    QVERIFY(!TrashMetadataCodec::encode(
        QStringLiteral("/other/source"), location, QDateTime::currentDateTime(),
        destination).ok());
    QCOMPARE(destination, QByteArray("unchanged"));
    QString invalid = QStringLiteral("/volume/");
    invalid.append(QChar(0xd800));
    QVERIFY(!TrashMetadataCodec::encode(invalid, location,
        QDateTime::currentDateTime(), destination).ok());
    QCOMPARE(destination, QByteArray("unchanged"));
  }
};
QTEST_GUILESS_MAIN(TrashMetadataTest)
#include "tst_trash_metadata.moc"
