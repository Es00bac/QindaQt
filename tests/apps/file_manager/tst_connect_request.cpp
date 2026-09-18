// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/connect_request.h"

#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] ConnectRequest sftpTo(const QString &host, const QString &path) {
  ConnectRequest request;
  request.scheme = QStringLiteral("sftp");
  request.host = host;
  request.remotePath = path;
  return request;
}

} // namespace

class TestConnectRequest final : public QObject {
  Q_OBJECT

private slots:
  void buildsTheLaptopLocations();
  void derivesAReadableNameAndTrimsInput();
  void normalizesScheme_data();
  void normalizesScheme();
  void refusesEveryUnusableField();
  void refusesUserinfoInTheHostField();
};

void TestConnectRequest::buildsTheLaptopLocations() {
  // The two locations Checkpoint L expects on the laptop.
  const auto storage = buildNetworkLocation(
      sftpTo(QStringLiteral("qinda"), QStringLiteral("/mnt/storage")));
  QVERIFY2(storage.ok(), qPrintable(storage.message));
  QCOMPARE(storage.record.url.toString(), QStringLiteral("sftp://qinda/mnt/storage"));
  QCOMPARE(storage.record.name, QStringLiteral("storage on qinda"));
  QVERIFY(storage.record.showInPlaces);

  const auto home = buildNetworkLocation(
      sftpTo(QStringLiteral("qinda"), QStringLiteral("/home/cabewse")));
  QVERIFY2(home.ok(), qPrintable(home.message));
  QCOMPARE(home.record.url.toString(), QStringLiteral("sftp://qinda/home/cabewse"));
  QCOMPARE(home.record.id, home.record.url.toString());
}

void TestConnectRequest::derivesAReadableNameAndTrimsInput() {
  ConnectRequest request = sftpTo(QStringLiteral("  QINDA  "),
                                  QStringLiteral("  mnt/storage  "));
  request.displayName = QStringLiteral("  Storage (desktop)  ");
  request.port = QStringLiteral(" 2222 ");
  const auto built = buildNetworkLocation(request);
  QVERIFY2(built.ok(), qPrintable(built.message));
  // A leading slash is supplied, the host is lower-cased by canonicalization,
  // and the given name wins over the derived one.
  QCOMPARE(built.record.url.toString(), QStringLiteral("sftp://qinda:2222/mnt/storage"));
  QCOMPARE(built.record.name, QStringLiteral("Storage (desktop)"));

  // A bare authority has no folder to name itself after, so the host is it.
  const auto bare = buildNetworkLocation(sftpTo(QStringLiteral("qinda"), QString()));
  QVERIFY2(bare.ok(), qPrintable(bare.message));
  QCOMPARE(bare.record.name, QStringLiteral("qinda"));
  QCOMPARE(bare.record.url.toString(), QStringLiteral("sftp://qinda"));
}

void TestConnectRequest::normalizesScheme_data() {
  QTest::addColumn<QString>("scheme");
  QTest::addColumn<bool>("accepted");
  QTest::newRow("sftp") << QStringLiteral("sftp") << true;
  QTest::newRow("smb") << QStringLiteral("smb") << true;
  QTest::newRow("SFTP") << QStringLiteral("SFTP") << true;
  QTest::newRow("ftp") << QStringLiteral("ftp") << false;
  QTest::newRow("nfs") << QStringLiteral("nfs") << false;
  QTest::newRow("file") << QStringLiteral("file") << false;
  QTest::newRow("empty") << QString() << false;
}

void TestConnectRequest::normalizesScheme() {
  QFETCH(QString, scheme);
  QFETCH(bool, accepted);
  ConnectRequest request = sftpTo(QStringLiteral("qinda"), QStringLiteral("/mnt"));
  request.scheme = scheme;
  const auto built = buildNetworkLocation(request);
  QCOMPARE(built.ok(), accepted);
  if (!accepted) {
    QCOMPARE(built.error, ConnectRequestError::UnsupportedScheme);
    QVERIFY(!built.message.isEmpty());
  }
}

void TestConnectRequest::refusesEveryUnusableField() {
  const auto refuse = [](const ConnectRequest &request, ConnectRequestError expected) {
    const auto built = buildNetworkLocation(request);
    QVERIFY(!built.ok());
    QCOMPARE(built.error, expected);
    QVERIFY(!built.message.isEmpty());
    // A refusal must carry no half-built record.
    QVERIFY(built.record.url.isEmpty());
    QVERIFY(built.record.id.isEmpty());
  };

  refuse(sftpTo(QStringLiteral("   "), QStringLiteral("/mnt")),
         ConnectRequestError::MissingHost);
  refuse(sftpTo(QStringLiteral("qin da"), QStringLiteral("/mnt")),
         ConnectRequestError::InvalidHost);
  refuse(sftpTo(QStringLiteral("qinda/mnt"), QString()),
         ConnectRequestError::InvalidHost);

  ConnectRequest badPort = sftpTo(QStringLiteral("qinda"), QStringLiteral("/mnt"));
  badPort.port = QStringLiteral("70000");
  refuse(badPort, ConnectRequestError::InvalidPort);
  badPort.port = QStringLiteral("0");
  refuse(badPort, ConnectRequestError::InvalidPort);
  badPort.port = QStringLiteral("twenty-two");
  refuse(badPort, ConnectRequestError::InvalidPort);

  refuse(sftpTo(QStringLiteral("qinda"), QStringLiteral("/mnt/../etc")),
         ConnectRequestError::InvalidPath);

  ConnectRequest longName = sftpTo(QStringLiteral("qinda"), QStringLiteral("/mnt"));
  longName.displayName = QString(257, QLatin1Char('n'));
  refuse(longName, ConnectRequestError::InvalidName);
}

void TestConnectRequest::refusesUserinfoInTheHostField() {
  // AGENT-GUARD: the dialog has no credential fields, so the host field is
  // the only way userinfo could be smuggled in. It must be refused before
  // QUrl parses it into an authority the user did not intend.
  const auto built = buildNetworkLocation(
      sftpTo(QStringLiteral("cabewse:secret@qinda"), QStringLiteral("/mnt")));
  QVERIFY(!built.ok());
  QCOMPARE(built.error, ConnectRequestError::InvalidHost);
  QVERIFY(built.record.url.isEmpty());

  const auto userOnly = buildNetworkLocation(
      sftpTo(QStringLiteral("cabewse@qinda"), QStringLiteral("/mnt")));
  QVERIFY(!userOnly.ok());
  QCOMPARE(userOnly.error, ConnectRequestError::InvalidHost);
}

QTEST_MAIN(TestConnectRequest)
#include "tst_connect_request.moc"
