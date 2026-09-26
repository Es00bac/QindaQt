// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "compat_refresh_job.h"
#include "compat_refresher.h"
#include "job_fakes.h"

using namespace QindaQt::QindaLutris;
using QindaQt::QindaLutris::TestSupport::FakeDownloader;

// ADR-0275 section 3: "Check for newer information" downloads the QindaQt
// release file and its checksum, and keeps the document only when it is
// intact, loads, and is newer than what is in use. Downloads are scripted.
class tst_compat_refresh : public QObject {
  Q_OBJECT

  static QByteArray document(const QString &generated) {
    const QJsonObject doc{
        {QStringLiteral("schema"), QStringLiteral("qindalutris-compat-db")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("generated"), generated},
        {QStringLiteral("sources"), QJsonArray()},
        {QStringLiteral("defaults"), QJsonObject{{QStringLiteral("recommendedBuild"), QString()}}},
        {QStringLiteral("builds"), QJsonObject()},
        {QStringLiteral("games"), QJsonArray()}};
    return QJsonDocument(doc).toJson(QJsonDocument::Compact);
  }

  static QByteArray sums(const QByteArray &body) {
    return QCryptographicHash::hash(body, QCryptographicHash::Sha512).toHex() +
           "  compat-db-v1.json\n";
  }

  static CompatDatabase database(const QString &generated) {
    const auto parsed = parseCompatDocument(document(generated));
    return parsed ? CompatDatabase::fromDocument(*parsed).value_or(CompatDatabase{})
                  : CompatDatabase{};
  }

  struct Rig {
    QTemporaryDir dir;
    CompatDatabase inUse;
    FakeDownloader *downloader = nullptr;
    std::unique_ptr<CompatRefresher> refresher;
    QString refreshedPath() const { return dir.filePath(QStringLiteral("data/compat-db-v1.json")); }
  };

  static void setUp(Rig &rig, const QString &inUse) {
    rig.inUse = database(inUse);
    QVERIFY(rig.inUse.isLoaded());
    auto downloader = std::make_unique<FakeDownloader>();
    rig.downloader = downloader.get();
    rig.refresher = std::make_unique<CompatRefresher>(&rig.inUse, rig.refreshedPath(),
                                                      std::move(downloader));
  }

  static void run(Rig &rig) {
    rig.refresher->checkForNewer();
    QVERIFY(rig.refresher->busy());
    QTRY_VERIFY(!rig.refresher->busy());
  }

private Q_SLOTS:
  void urlsAreTheQindaQtRelease() {
    QCOMPARE(compatDbRefreshUrl().toString(),
             QStringLiteral("https://github.com/Es00bac/QindaQt/releases/download/compat-db/"
                            "compat-db-v1.json"));
    QVERIFY(compatDbChecksumUrl().toString().endsWith(QStringLiteral(".json.sha512sum")));
  }

  void newerDocumentIsKeptAndUsed() {
    Rig rig;
    setUp(rig, QStringLiteral("2026-09-01T00:00:00Z"));
    const QByteArray body = document(QStringLiteral("2026-09-20T00:00:00Z"));
    rig.downloader->serve(compatDbChecksumUrl(), sums(body));
    rig.downloader->serve(compatDbRefreshUrl(), body);
    QSignalSpy changed(rig.refresher.get(), &CompatRefresher::changed);
    run(rig);
    QCOMPARE(rig.refresher->message(), QStringLiteral("Updated to information from 2026-09-20."));
    QCOMPARE(rig.inUse.generated().date(), QDate(2026, 9, 20));
    QFile kept(rig.refreshedPath());
    QVERIFY(kept.open(QIODevice::ReadOnly));
    QCOMPARE(kept.readAll(), body);
    QVERIFY(changed.size() >= 2);
    QCOMPARE(rig.downloader->requested.first(), compatDbChecksumUrl());
  }

  void sameDateChangesNothing() {
    Rig rig;
    setUp(rig, QStringLiteral("2026-09-20T00:00:00Z"));
    const QByteArray body = document(QStringLiteral("2026-09-20T00:00:00Z"));
    rig.downloader->serve(compatDbChecksumUrl(), sums(body));
    rig.downloader->serve(compatDbRefreshUrl(), body);
    run(rig);
    QCOMPARE(rig.refresher->message(), QStringLiteral("You already have the newest information."));
    QVERIFY(!QFile::exists(rig.refreshedPath()));
  }

  void olderDocumentIsIgnored() {
    Rig rig;
    setUp(rig, QStringLiteral("2026-09-20T00:00:00Z"));
    const QByteArray body = document(QStringLiteral("2026-09-01T00:00:00Z"));
    rig.downloader->serve(compatDbChecksumUrl(), sums(body));
    rig.downloader->serve(compatDbRefreshUrl(), body);
    run(rig);
    QCOMPARE(rig.inUse.generated().date(), QDate(2026, 9, 20));
    QVERIFY(!QFile::exists(rig.refreshedPath()));
  }

  void checksumMismatchIsDamaged() {
    Rig rig;
    setUp(rig, QStringLiteral("2026-09-01T00:00:00Z"));
    const QByteArray body = document(QStringLiteral("2026-09-20T00:00:00Z"));
    rig.downloader->serve(compatDbChecksumUrl(), sums(body + " "));
    rig.downloader->serve(compatDbRefreshUrl(), body);
    run(rig);
    QCOMPARE(rig.refresher->message(),
             QStringLiteral("The downloaded information was damaged; nothing changed."));
    QCOMPARE(rig.inUse.generated().date(), QDate(2026, 9, 1));
  }

  void unreadableDocumentChangesNothing() {
    Rig rig;
    setUp(rig, QStringLiteral("2026-09-01T00:00:00Z"));
    const QByteArray body = "{\"schema\": \"something-else\"}";
    rig.downloader->serve(compatDbChecksumUrl(), sums(body));
    rig.downloader->serve(compatDbRefreshUrl(), body);
    run(rig);
    QVERIFY(rig.refresher->message().contains(QStringLiteral("could not be read")));
    QVERIFY(!QFile::exists(rig.refreshedPath()));
  }

  void offlineIsOneSentence() {
    Rig rig;
    setUp(rig, QStringLiteral("2026-09-01T00:00:00Z"));
    run(rig); // every URL answers 404
    QVERIFY(rig.refresher->message().startsWith(QStringLiteral("Newer information could not be downloaded")));
    QCOMPARE(rig.downloader->requested.size(), 1);
  }
};

QTEST_GUILESS_MAIN(tst_compat_refresh)
#include "tst_compat_refresh.moc"
