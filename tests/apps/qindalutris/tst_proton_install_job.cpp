// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCryptographicHash>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "job_fakes.h"
#include "proton_install_job.h"

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

namespace {

const QString kTool = QStringLiteral("GE-Proton99-1-x86_64");

GeProtonRelease testRelease() {
  GeProtonRelease release;
  release.tagName = QStringLiteral("GE-Proton99-1");
  release.toolName = kTool;
  release.tarballName = kTool + QStringLiteral(".tar.gz");
  release.checksumName = kTool + QStringLiteral(".sha512sum");
  const QString base = QStringLiteral(
      "https://github.com/GloriousEggroll/proton-ge-custom/releases/download/GE-Proton99-1/");
  release.tarballUrl = QUrl(base + release.tarballName);
  release.checksumUrl = QUrl(base + release.checksumName);
  return release;
}

// Builds a real .tar.gz with the host's tar. extraArgs go before the member
// list so tests can add -P (keep '../' names) for escape fixtures.
QByteArray makeTarball(const QString &workDir, const QStringList &members,
                       const QStringList &extraArgs = {}) {
  const QString out = workDir + QStringLiteral("/out.tar.gz");
  QFile::remove(out);
  QProcess tar;
  QStringList args{QStringLiteral("-czf"), out};
  args << extraArgs << QStringLiteral("-C") << workDir + QStringLiteral("/src") << members;
  tar.start(QStringLiteral("tar"), args);
  if (!tar.waitForFinished(20000) || tar.exitCode() != 0) {
    qWarning() << "tar failed" << tar.readAllStandardError();
    return {};
  }
  QFile file(out);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QByteArray sumFor(const QByteArray &tarball, const QString &name) {
  return QCryptographicHash::hash(tarball, QCryptographicHash::Sha512).toHex() + "  " +
         name.toUtf8() + "\n";
}

struct Fixture {
  QTemporaryDir dir{QDir::homePath() + QStringLiteral("/proton-XXXXXX")};
  QString root = dir.filePath(QStringLiteral("compatibilitytools.d"));
  QString work = dir.filePath(QStringLiteral("work"));
  FakeDownloader downloader;
  QProcessRunner runner;
  ProtonInstallJob job{root, &downloader, &runner};
  std::optional<ProtonJobResult> result;

  Fixture() {
    QDir().mkpath(work + QStringLiteral("/src/") + kTool + QStringLiteral("/files"));
    writeFile(work + QStringLiteral("/src/") + kTool + QStringLiteral("/proton"), 64);
    writeFile(work + QStringLiteral("/src/") + kTool + QStringLiteral("/compatibilitytool.vdf"), 32);
    QObject::connect(&job, &ProtonInstallJob::finished, [this](const ProtonJobResult &r) { result = r; });
  }

  void serve(const QByteArray &tarball, const QByteArray &sums) {
    const GeProtonRelease release = testRelease();
    downloader.serve(release.tarballUrl, tarball);
    downloader.serve(release.checksumUrl, sums);
  }

  bool run() {
    job.start(testRelease());
    return QTest::qWaitFor([this] { return result.has_value(); }, 20000);
  }

  QStringList rootEntries() const {
    return QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
  }
};

} // namespace

class tst_proton_install_job : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() {
    if (QStandardPaths::findExecutable(QStringLiteral("tar")).isEmpty()) {
      QSKIP("tar is not installed on this host");
    }
  }

  void verifiedArchiveIsInstalledAtomically() {
    Fixture f;
    const QByteArray tarball = makeTarball(f.work, {kTool});
    QVERIFY(!tarball.isEmpty());
    f.serve(tarball, sumFor(tarball, kTool + QStringLiteral(".tar.gz")));
    QList<double> fractions;
    QObject::connect(&f.job, &ProtonInstallJob::progress,
                     [&fractions](double fraction, const QString &) { fractions.append(fraction); });
    QVERIFY(f.run());
    QVERIFY2(f.result->ok, qPrintable(f.result->message + QLatin1Char('\n') + f.job.detailsText()));
    QCOMPARE(f.result->toolName, kTool);
    QCOMPARE(f.result->installedPath, f.root + QLatin1Char('/') + kTool);
    QVERIFY(QFileInfo(f.result->installedPath + QStringLiteral("/proton")).isFile());
    // No staging left behind: the root holds exactly the new build.
    QCOMPARE(f.rootEntries(), QStringList{kTool});
    QCOMPARE(f.downloader.requested.size(), 2);
    QVERIFY(!fractions.isEmpty());
    QCOMPARE(fractions.last(), 1.0);
    QVERIFY(std::is_sorted(fractions.cbegin(), fractions.cend()));
  }

  void checksumMismatchIsRefused() {
    Fixture f;
    const QByteArray tarball = makeTarball(f.work, {kTool});
    f.serve(tarball, sumFor(QByteArray("something else"), kTool + QStringLiteral(".tar.gz")));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.contains(QStringLiteral("safety check")));
    QVERIFY(f.job.detailsText().contains(QStringLiteral("SHA-512 mismatch")));
    QVERIFY(f.rootEntries().isEmpty());
  }

  void checksumForAnotherFileIsRefused() {
    Fixture f;
    const QByteArray tarball = makeTarball(f.work, {kTool});
    f.serve(tarball, sumFor(tarball, QStringLiteral("GE-Proton99-1-aarch64.tar.gz")));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.rootEntries().isEmpty());
  }

  void archiveEscapingItsFolderIsRefused() {
    Fixture f;
    writeFile(f.work + QStringLiteral("/escape.txt"), 8);
    // -P keeps the '../' member name, exactly what a hostile archive carries.
    const QByteArray tarball =
        makeTarball(f.work, {kTool, QStringLiteral("../escape.txt")}, {QStringLiteral("-P")});
    QVERIFY(!tarball.isEmpty());
    f.serve(tarball, sumFor(tarball, kTool + QStringLiteral(".tar.gz")));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.contains(QStringLiteral("expected shape")));
    QVERIFY(f.job.detailsText().contains(QStringLiteral("outside its folder")));
    QVERIFY(f.rootEntries().isEmpty());
    QVERIFY(!QFile::exists(f.dir.filePath(QStringLiteral("escape.txt"))));
  }

  void archiveWithAnotherTopFolderIsRefused() {
    Fixture f;
    QDir(f.work + QStringLiteral("/src")).rename(kTool, QStringLiteral("GE-Proton99-1"));
    const QByteArray tarball = makeTarball(f.work, {QStringLiteral("GE-Proton99-1")});
    f.serve(tarball, sumFor(tarball, kTool + QStringLiteral(".tar.gz")));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.job.detailsText().contains(QStringLiteral("does not match")));
    QVERIFY(f.rootEntries().isEmpty());
  }

  void existingBuildIsNeverReplaced() {
    Fixture f;
    QDir().mkpath(f.root + QLatin1Char('/') + kTool);
    writeFile(f.root + QLatin1Char('/') + kTool + QStringLiteral("/marker"), 1);
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QCOMPARE(f.result->message, kTool + QStringLiteral(" is already installed."));
    QVERIFY(f.downloader.requested.isEmpty());
    QVERIFY(QFile::exists(f.root + QLatin1Char('/') + kTool + QStringLiteral("/marker")));
  }

  void downloadFailuresArePlain_data() {
    QTest::addColumn<int>("mode");
    QTest::addColumn<QString>("detail");
    QTest::newRow("http error") << int(FakeDownloader::Mode::HttpError) << QStringLiteral("HTTP 404");
    QTest::newRow("truncated") << int(FakeDownloader::Mode::Truncated) << QStringLiteral("cut short");
    QTest::newRow("bad redirect") << int(FakeDownloader::Mode::BadRedirect)
                                  << QStringLiteral("downloads.evil.example");
  }
  void downloadFailuresArePlain() {
    QFETCH(int, mode);
    QFETCH(QString, detail);
    Fixture f;
    f.downloader.script(testRelease().tarballUrl, FakeDownloader::Mode(mode));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.startsWith(QStringLiteral("The Proton build could not be downloaded.")));
    QVERIFY(f.job.detailsText().contains(detail));
    QVERIFY(f.rootEntries().isEmpty());
  }

  void nonAllowlistedReleaseIsRefusedBeforeDownloading() {
    Fixture f;
    GeProtonRelease release = testRelease();
    release.tarballUrl = QUrl(QStringLiteral("https://mirror.evil.example/x.tar.gz"));
    f.job.start(release);
    QVERIFY(f.result.has_value());
    QVERIFY(!f.result->ok);
    QVERIFY(f.downloader.requested.isEmpty());
  }

  void cancelDuringDownloadCleansUp() {
    Fixture f;
    f.downloader.script(testRelease().tarballUrl, FakeDownloader::Mode::Hang);
    f.job.start(testRelease());
    QVERIFY(f.job.isRunning());
    QVERIFY(f.rootEntries().first().startsWith(QStringLiteral(".qindalutris-staging-")));
    f.job.cancel();
    QVERIFY(f.result.has_value());
    QVERIFY(f.result->cancelled);
    QVERIFY(!f.job.isRunning());
    QCOMPARE(f.downloader.cancels, 1);
    QVERIFY(f.rootEntries().isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_proton_install_job)
#include "tst_proton_install_job.moc"
