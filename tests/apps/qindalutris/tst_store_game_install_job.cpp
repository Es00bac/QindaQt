// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "job_fakes.h"
#include "store_game_install_job.h"

using namespace QindaQt::QindaLutris;
using QindaQt::QindaLutris::TestSupport::FakeRunner;

// ADR-0275 section 8: one owned game downloaded by its store client, with
// progress from the client's own lines, then its program located. The
// runner is scripted; no client, network or store account is used.
class tst_store_game_install_job : public QObject {
  Q_OBJECT

  QTemporaryDir m_dir;

  StoreGameInstallRequest request(StoreClient client, const QString &id) const {
    StoreGameInstallRequest out;
    out.client = client;
    out.binaries = {QStringLiteral("/usr/bin/legendary"), QStringLiteral("/usr/bin/gogdl"),
                    QStringLiteral("/usr/bin/nile")};
    out.storesRoot = m_dir.filePath(QStringLiteral("stores"));
    out.gameId = id;
    out.title = QStringLiteral("Some Game");
    out.baseDir = m_dir.filePath(QStringLiteral("Games"));
    return out;
  }

  static ProcessRunResult ok(const QByteArray &out = {}) {
    ProcessRunResult result;
    result.started = true;
    result.exitCode = 0;
    result.standardOutput = out;
    return result;
  }

  static void touch(const QString &path, const QByteArray &content = {}) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
  }

private Q_SLOTS:
  void initTestCase() { QVERIFY(m_dir.isValid()); }

  void epicDownloadsThenLocates() {
    const QString exe = m_dir.filePath(QStringLiteral("Games/RocketLeague/Binaries/RL.exe"));
    touch(exe);
    FakeRunner runner;
    runner.stream = [](const ProcessRunSpec &spec) {
      return spec.arguments.contains(QStringLiteral("install"))
                 ? QByteArray("[DLManager] INFO: = Progress: 40.00% (4/10)\r"
                              "[DLManager] INFO: = Progress: 80.00% (8/10)\n")
                 : QByteArray();
    };
    const QString dir = m_dir.filePath(QStringLiteral("Games/RocketLeague"));
    runner.respond = [dir](const ProcessRunSpec &spec) {
      if (spec.arguments.contains(QStringLiteral("list-installed"))) {
        return ok("[{\"app_name\": \"Sugar\", \"install_path\": \"" + dir.toUtf8() +
                  "\", \"executable\": \"Binaries\\\\RL.exe\"}]");
      }
      return ok();
    };
    StoreGameInstallJob job(&runner);
    QSignalSpy progress(&job, &StoreGameInstallJob::progressChanged);
    QSignalSpy finished(&job, &StoreGameInstallJob::finished);
    job.start(request(StoreClient::Epic, QStringLiteral("Sugar")));
    QVERIFY(finished.wait(2000));
    const auto result = finished.first().first().value<StoreGameInstallResult>();
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.install.executable, exe);
    QCOMPARE(runner.specs.size(), 2);
    QCOMPARE(runner.specs.first().arguments.at(1), QStringLiteral("install"));
    bool sawEighty = false;
    for (const QList<QVariant> &row : progress) {
      sawEighty = sawEighty || qFuzzyCompare(row.first().toDouble(), 0.8);
    }
    QVERIFY(sawEighty);
    QVERIFY(job.detailsText().contains(QStringLiteral("installed:")));
  }

  void gogReadsInfoFile() {
    const QString folder = m_dir.filePath(QStringLiteral("Games/Stardew"));
    touch(folder + QStringLiteral("/Stardew.exe"));
    touch(folder + QStringLiteral("/goggame-1453375253.info"),
          "{\"playTasks\": [{\"type\": \"FileTask\", \"isPrimary\": true, \"path\": \"Stardew.exe\"}]}");
    FakeRunner runner;
    StoreGameInstallJob job(&runner);
    QSignalSpy finished(&job, &StoreGameInstallJob::finished);
    job.start(request(StoreClient::Gog, QStringLiteral("1453375253")));
    QVERIFY(finished.wait(2000));
    const auto result = finished.first().first().value<StoreGameInstallResult>();
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.install.installDir, folder);
    QCOMPARE(runner.specs.size(), 1); // no locate run for GOG
  }

  void failedDownloadIsOneSentence() {
    FakeRunner runner;
    runner.stream = [](const ProcessRunSpec &) {
      return QByteArray("[cli] ERROR: Not enough available disk space!\n");
    };
    runner.result.exitCode = 1;
    StoreGameInstallJob job(&runner);
    QSignalSpy finished(&job, &StoreGameInstallJob::finished);
    job.start(request(StoreClient::Amazon, QStringLiteral("amzn1.x")));
    QVERIFY(finished.wait(2000));
    const auto result = finished.first().first().value<StoreGameInstallResult>();
    QVERIFY(!result.ok);
    QVERIFY(result.message.contains(QStringLiteral("could not be downloaded")));
    QVERIFY(job.detailsText().contains(QStringLiteral("Not enough available disk space")));
  }

  void missingProgramFails() {
    FakeRunner runner;
    runner.respond = [](const ProcessRunSpec &spec) {
      return spec.arguments.contains(QStringLiteral("--json"))
                 ? ok("{\"command\": {\"instruction\": \"/nonexistent/g/x.exe\"}, "
                      "\"game_directory\": \"/nonexistent/g\"}")
                 : ok();
    };
    StoreGameInstallJob job(&runner);
    QSignalSpy finished(&job, &StoreGameInstallJob::finished);
    job.start(request(StoreClient::Amazon, QStringLiteral("amzn1.x")));
    QVERIFY(finished.wait(2000));
    QVERIFY(!finished.first().first().value<StoreGameInstallResult>().ok);
  }

  void cancelReportsStopped() {
    FakeRunner runner;
    runner.hang = true;
    StoreGameInstallJob job(&runner);
    QSignalSpy finished(&job, &StoreGameInstallJob::finished);
    job.start(request(StoreClient::Epic, QStringLiteral("Sugar")));
    QVERIFY(job.isRunning());
    job.cancel();
    QVERIFY(finished.wait(2000));
    QVERIFY(finished.first().first().value<StoreGameInstallResult>().cancelled);
    QVERIFY(!job.isRunning());
  }

  void refusesUnsafeIdWithoutRunning() {
    FakeRunner runner;
    StoreGameInstallJob job(&runner);
    QSignalSpy finished(&job, &StoreGameInstallJob::finished);
    job.start(request(StoreClient::Epic, QStringLiteral("--base-path")));
    QCOMPARE(finished.size(), 1);
    QVERIFY(runner.specs.isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_store_game_install_job)
#include "tst_store_game_install_job.moc"
