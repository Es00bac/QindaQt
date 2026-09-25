// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QNetworkProxy>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include "network_downloader.h"

#include <optional>

using namespace QindaQt::QindaLutris;

// The production NetworkDownloader end to end against a local plain-HTTP
// server on 127.0.0.1. Only the URL policy is swapped (the test-only seam),
// so redirects, caps, stalls and truncation run through the real code.
namespace {

class LocalServer final : public QObject {
public:
  QTcpServer server;
  QList<QByteArray> requests;

  LocalServer() {
    server.listen(QHostAddress::LocalHost);
    connect(&server, &QTcpServer::newConnection, this, [this] {
      while (QTcpSocket *socket = server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { handle(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
      }
    });
  }

  QUrl url(const QString &path) const {
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
  }

private:
  void reply(QTcpSocket *socket, const QByteArray &status, const QByteArray &headers,
             const QByteArray &body, bool close = true) {
    socket->write("HTTP/1.1 " + status + "\r\n" + headers + (close ? "Connection: close\r\n" : "") +
                  "\r\n" + body);
    socket->flush();
    if (close) {
      socket->disconnectFromHost();
    }
  }

  void handle(QTcpSocket *socket) {
    const QByteArray head = socket->readAll();
    const QByteArray path = head.split(' ').value(1);
    requests.append(path);
    const QByteArray port = QByteArray::number(server.serverPort());
    if (path == "/ok") {
      reply(socket, "200 OK", "Content-Length: 1000\r\n", QByteArray(1000, 'A'));
    } else if (path == "/redir-ok") {
      reply(socket, "302 Found", "Location: /ok\r\nContent-Length: 0\r\n", {});
    } else if (path == "/redir-off-list") {
      reply(socket, "302 Found", "Location: http://localhost:" + port + "/ok\r\nContent-Length: 0\r\n", {});
    } else if (path == "/redir-loop") {
      reply(socket, "302 Found", "Location: /redir-loop\r\nContent-Length: 0\r\n", {});
    } else if (path == "/biglen") {
      reply(socket, "200 OK", "Content-Length: 5000\r\n", QByteArray(5000, 'B'));
    } else if (path == "/big") {
      reply(socket, "200 OK", {}, QByteArray(5000, 'B'));
    } else if (path == "/short") {
      reply(socket, "200 OK", "Content-Length: 1000\r\n", QByteArray(400, 'S'));
    } else if (path == "/stall") {
      reply(socket, "200 OK", "Content-Length: 1000\r\n", QByteArray(10, 's'), false);
    } else if (path == "/500") {
      reply(socket, "500 Oops", "Content-Length: 3\r\n", "err");
    } else if (path == "/empty") {
      reply(socket, "200 OK", "Content-Length: 0\r\n", {});
    } else {
      reply(socket, "404 Not Found", "Content-Length: 0\r\n", {});
    }
  }
};

struct Download {
  QTemporaryDir dir{QDir::homePath() + QStringLiteral("/net-XXXXXX")};
  QString destination = dir.filePath(QStringLiteral("file.bin"));
  std::optional<std::pair<bool, QString>> result;
  NetworkDownloader downloader;

  explicit Download(DownloadLimits limits = {}) : downloader(limits) {
    downloader.setUrlPolicyForTesting([](const QUrl &url) {
      return url.scheme() == QLatin1String("http") && url.host() == QLatin1String("127.0.0.1");
    });
    QObject::connect(&downloader, &Downloader::finished,
                     [this](bool ok, const QString &reason) { result = std::make_pair(ok, reason); });
  }

  bool run(const QUrl &url) {
    downloader.start(url, destination);
    return QTest::qWaitFor([this] { return result.has_value(); }, 10000);
  }

  bool leftNothing() const {
    return !QFile::exists(destination) && !QFile::exists(destination + QStringLiteral(".part"));
  }
};

} // namespace

class tst_network_downloader : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() { QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy); }

  void completeDownloadIsRenamedIntoPlace() {
    LocalServer server;
    Download d;
    QVERIFY(d.run(server.url(QStringLiteral("/ok"))));
    QVERIFY2(d.result->first, qPrintable(d.result->second));
    QCOMPARE(QFileInfo(d.destination).size(), qint64(1000));
    QVERIFY(!QFile::exists(d.destination + QStringLiteral(".part")));
  }

  void allowedRedirectIsFollowed() {
    LocalServer server;
    Download d;
    QVERIFY(d.run(server.url(QStringLiteral("/redir-ok"))));
    QVERIFY2(d.result->first, qPrintable(d.result->second));
    QCOMPARE(server.requests, QList<QByteArray>({"/redir-ok", "/ok"}));
  }

  void redirectOffThePolicyIsRefusedBeforeItIsFollowed() {
    LocalServer server;
    Download d;
    QVERIFY(d.run(server.url(QStringLiteral("/redir-off-list"))));
    QVERIFY(!d.result->first);
    QVERIFY(d.result->second.contains(QStringLiteral("localhost")));
    QCOMPARE(server.requests, QList<QByteArray>{"/redir-off-list"});
    QVERIFY(d.leftNothing());
  }

  void redirectLoopStopsAtTheGuardLimit() {
    LocalServer server;
    DownloadLimits limits;
    limits.maxRedirects = 3;
    Download d(limits);
    QVERIFY(d.run(server.url(QStringLiteral("/redir-loop"))));
    QVERIFY(!d.result->first);
    QVERIFY2(d.result->second.contains(QStringLiteral("too many times")), qPrintable(d.result->second));
    QCOMPARE(server.requests.size(), 4);
    QVERIFY(d.leftNothing());
  }

  void announcedOversizeIsRefused() {
    LocalServer server;
    DownloadLimits limits;
    limits.maxBytes = 2000;
    Download d(limits);
    QVERIFY(d.run(server.url(QStringLiteral("/biglen"))));
    QVERIFY(!d.result->first);
    QVERIFY(d.result->second.contains(QStringLiteral("larger")));
    QVERIFY(d.leftNothing());
  }

  void unannouncedOversizeIsCutOff() {
    LocalServer server;
    DownloadLimits limits;
    limits.maxBytes = 2000;
    Download d(limits);
    QVERIFY(d.run(server.url(QStringLiteral("/big"))));
    QVERIFY(!d.result->first);
    QVERIFY2(d.result->second.contains(QStringLiteral("limit")), qPrintable(d.result->second));
    QVERIFY(d.leftNothing());
  }

  void truncatedTransferIsRefused() {
    LocalServer server;
    Download d;
    QVERIFY(d.run(server.url(QStringLiteral("/short"))));
    QVERIFY(!d.result->first);
    QVERIFY2(d.result->second.contains(QStringLiteral("400")), qPrintable(d.result->second));
    QVERIFY(d.leftNothing());
  }

  void stalledTransferTimesOut() {
    LocalServer server;
    DownloadLimits limits;
    limits.stallTimeoutMs = 400;
    Download d(limits);
    QVERIFY(d.run(server.url(QStringLiteral("/stall"))));
    QVERIFY(!d.result->first);
    QVERIFY2(d.result->second.contains(QStringLiteral("stopped sending")), qPrintable(d.result->second));
    QVERIFY(d.leftNothing());
  }

  void httpErrorAndEmptyBodyAreRefused() {
    LocalServer server;
    Download error;
    QVERIFY(error.run(server.url(QStringLiteral("/500"))));
    QVERIFY(!error.result->first);
    QVERIFY(error.result->second.contains(QStringLiteral("500")));
    QVERIFY(error.leftNothing());
    Download empty;
    QVERIFY(empty.run(server.url(QStringLiteral("/empty"))));
    QVERIFY(!empty.result->first);
    QVERIFY(empty.leftNothing());
  }

  void cancelMidTransferLeavesNothing() {
    LocalServer server;
    Download d;
    d.downloader.start(server.url(QStringLiteral("/stall")), d.destination);
    QVERIFY(QTest::qWaitFor([&] { return !server.requests.isEmpty(); }, 5000));
    d.downloader.cancel();
    QTest::qWait(100);
    QVERIFY(!d.result.has_value());
    QVERIFY(d.leftNothing());
  }

  void productionPolicyStillRefusesThisServer() {
    LocalServer server;
    NetworkDownloader production;
    std::optional<bool> ok;
    connect(&production, &Downloader::finished, this, [&](bool success, const QString &) { ok = success; });
    QTemporaryDir dir(QDir::homePath() + QStringLiteral("/net-XXXXXX"));
    production.start(server.url(QStringLiteral("/ok")), dir.filePath(QStringLiteral("x")));
    QVERIFY(QTest::qWaitFor([&] { return ok.has_value(); }, 5000));
    QVERIFY(!*ok);
    QVERIFY(server.requests.isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_network_downloader)
#include "tst_network_downloader.moc"
