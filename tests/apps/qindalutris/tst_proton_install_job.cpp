// SPDX-License-Identifier: GPL-3.0-or-later
#include <QStandardPaths>
#include <QTest>

#include "proton_job_fixture.h"
#include "proton_removal.h"

#include <unistd.h>

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

// ProtonInstallJob happy path and refusals (ADR-0275 section 2). Cancel
// rows live in tst_proton_install_cancel.cpp.
class tst_proton_install_job : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() {
    if (QStandardPaths::findExecutable(QStringLiteral("tar")).isEmpty()) {
      QSKIP("tar is not installed on this host");
    }
  }

  void verifiedArchiveIsInstalledAtomically() {
    ProtonFixture f;
    f.serveBuild();
    QList<double> fractions;
    QObject::connect(&f.job, &ProtonInstallJob::progress,
                     [&fractions](double fraction, const QString &) { fractions.append(fraction); });
    QVERIFY(f.run());
    QVERIFY2(f.result->ok, qPrintable(f.result->message + QLatin1Char('\n') + f.job.detailsText()));
    QCOMPARE(f.result->installedPath, f.root + QLatin1Char('/') + kTool);
    QVERIFY(QFileInfo(f.result->installedPath + QStringLiteral("/proton")).isFile());
    QCOMPARE(f.rootEntries(), QStringList{kTool}); // no staging left behind
    QCOMPARE(f.downloader.requested.size(), 2);
    QCOMPARE(fractions.last(), 1.0);
    QVERIFY(std::is_sorted(fractions.cbegin(), fractions.cend()));
    QVERIFY(!f.job.isRunning());
  }

  void internalLinksSurviveAndReadOnlyFoldersCanBeRemoved() {
    ProtonFixture f;
    writeFile(f.source + QStringLiteral("/files/lib/libwine.so.1.0"), 128);
    QVERIFY(QFile::link(QStringLiteral("libwine.so.1.0"), f.source + QStringLiteral("/files/lib/libwine.so.1")));
    QCOMPARE(::link(QFile::encodeName(f.source + QStringLiteral("/files/lib/libwine.so.1.0")).constData(),
                    QFile::encodeName(f.source + QStringLiteral("/files/lib/copy.so")).constData()),
             0);
    writeFile(f.source + QStringLiteral("/ro/f"), 4);
    QFile::setPermissions(f.source + QStringLiteral("/ro"),
                          QFileDevice::ReadOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                              QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
    f.serveBuild();
    QFile::setPermissions(f.source + QStringLiteral("/ro"), QFileDevice::ReadOwner |
                                                                QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QVERIFY(f.run());
    QVERIFY2(f.result->ok, qPrintable(f.job.detailsText()));
    const QString installed = f.result->installedPath;
    QCOMPARE(QFileInfo(installed + QStringLiteral("/files/lib/libwine.so.1")).symLinkTarget(),
             installed + QStringLiteral("/files/lib/libwine.so.1.0"));

    ProtonRemovalRequest removal;
    removal.userRoot = f.root;
    removal.buildName = kTool;
    const ProtonRemovalResult removed = removeProtonBuild(removal);
    QVERIFY(removed.ok);
    QVERIFY2(removed.complete, qPrintable(removed.message));
    QVERIFY(sweepProtonTrash(f.root));
    QVERIFY(f.rootEntries().isEmpty());
  }

  void checksumMismatchIsRefused() {
    ProtonFixture f;
    const QByteArray tarball = makeTarball(f.work, {kTool});
    f.serve(tarball, sumFor(QByteArray("something else"), kTool + QStringLiteral(".tar.gz")));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.contains(QStringLiteral("safety check")));
    QVERIFY(f.job.detailsText().contains(QStringLiteral("SHA-512 mismatch")));
    QVERIFY(f.rootEntries().isEmpty());
  }

  void checksumForAnotherFileIsRefused() {
    ProtonFixture f;
    const QByteArray tarball = makeTarball(f.work, {kTool});
    f.serve(tarball, sumFor(tarball, QStringLiteral("GE-Proton99-1-aarch64.tar.gz")));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.rootEntries().isEmpty());
  }

  void archiveEscapingItsFolderIsRefusedBeforeExtraction() {
    ProtonFixture f;
    writeFile(f.work + QStringLiteral("/escape.txt"), 8);
    // -P keeps the '../' member name, exactly what a hostile archive carries.
    const QByteArray tarball =
        makeTarball(f.work, {kTool, QStringLiteral("../escape.txt")}, {QStringLiteral("-P")});
    QVERIFY(!tarball.isEmpty());
    f.serve(tarball, sumFor(tarball, kTool + QStringLiteral(".tar.gz")));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.contains(QStringLiteral("expected shape")));
    QVERIFY(!f.job.detailsText().contains(QStringLiteral("Archive listing accepted")));
    QVERIFY(f.rootEntries().isEmpty());
    // Relative to the extraction folder, '../escape.txt' would land in
    // staging (inside the root); relative to the root, beside it. Neither
    // exists anywhere in the fixture outside the tar sources.
    QVERIFY2(f.filesNamed(QStringLiteral("escape.txt")).isEmpty(),
             qPrintable(f.filesNamed(QStringLiteral("escape.txt")).join(QLatin1Char(' '))));
  }

  void archiveWithALinkOutsideIsRefused() {
    ProtonFixture f;
    QVERIFY(QFile::link(f.dir.filePath(QStringLiteral("outside")), f.source + QStringLiteral("/lnk")));
    f.serveBuild();
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.job.detailsText().contains(QStringLiteral("pointing outside")));
    QVERIFY(f.rootEntries().isEmpty());
  }

  void archiveWithAnotherTopFolderIsRefused() {
    ProtonFixture f;
    QDir(f.work + QStringLiteral("/src")).rename(kTool, QStringLiteral("GE-Proton99-1"));
    const QByteArray tarball = makeTarball(f.work, {QStringLiteral("GE-Proton99-1")});
    f.serve(tarball, sumFor(tarball, kTool + QStringLiteral(".tar.gz")));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.job.detailsText().contains(QStringLiteral("does not match")));
    QVERIFY(f.rootEntries().isEmpty());
  }

  void existingBuildIsNeverReplaced() {
    ProtonFixture f;
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
    ProtonFixture f;
    f.downloader.script(testRelease().tarballUrl, FakeDownloader::Mode(mode));
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.startsWith(QStringLiteral("The Proton build could not be downloaded.")));
    QVERIFY(f.job.detailsText().contains(detail));
    QVERIFY(f.rootEntries().isEmpty());
  }

  void releasesNotTiedToUpstreamAreRefused_data() {
    QTest::addColumn<QString>("tarballUrl");
    QTest::addColumn<QString>("checksumUrl");
    const QString base = QString::fromLatin1(kGeProtonDownloadPrefix) + QStringLiteral("GE-Proton99-1/");
    QTest::newRow("attacker fork") << QStringLiteral(
        "https://github.com/attacker/fork/releases/download/GE-Proton99-1/GE-Proton99-1-x86_64.tar.gz")
                                   << base + kTool + QStringLiteral(".sha512sum");
    QTest::newRow("checksum on another allowlisted host")
        << base + kTool + QStringLiteral(".tar.gz")
        << QStringLiteral("https://download.amazongames.com/GE-Proton99-1-x86_64.sha512sum");
    QTest::newRow("off-list host") << QStringLiteral("https://mirror.evil.example/x.tar.gz")
                                   << base + kTool + QStringLiteral(".sha512sum");
  }
  void releasesNotTiedToUpstreamAreRefused() {
    QFETCH(QString, tarballUrl);
    QFETCH(QString, checksumUrl);
    ProtonFixture f;
    GeProtonRelease release = testRelease();
    release.tarballUrl = QUrl(tarballUrl);
    release.checksumUrl = QUrl(checksumUrl);
    f.job.start(release);
    QVERIFY(!f.result.has_value()); // queued, never synchronous
    QVERIFY(f.job.isRunning());
    QVERIFY(f.wait());
    QVERIFY(!f.result->ok);
    QCOMPARE(f.result->message, QStringLiteral("This Proton release cannot be installed safely."));
    QVERIFY(f.downloader.requested.isEmpty());
    QVERIFY(f.rootEntries().isEmpty());
  }

  void notEnoughSpaceIsRefusedBeforeDownloading() {
    ProtonFixture f;
    GeProtonRelease release = testRelease();
    release.tarballBytes = 534 * 1024 * 1024;
    f.probe.freeBytes = qint64(1024) * 1024 * 1024;
    QVERIFY(f.run(release));
    QVERIFY(!f.result->ok);
    QVERIFY2(f.result->message.contains(QStringLiteral("not enough free disk space")),
             qPrintable(f.result->message));
    QVERIFY(f.downloader.requested.isEmpty());
    QCOMPARE(protonInstallSpaceNeeded(release.tarballBytes), 4 * release.tarballBytes);
    QVERIFY(protonInstallSpaceNeeded(-1) >= qint64(3) * 1024 * 1024 * 1024);
  }

  void tarFailureIsItsOwnMessage() {
    ProtonFixture f;
    f.job.setTarProgram(writeScript(f.dir.filePath(QStringLiteral("broken-tar")), "exit 2\n"));
    f.serveBuild();
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY2(f.result->message.contains(QStringLiteral("could not be unpacked")), qPrintable(f.result->message));
    QVERIFY(f.job.detailsText().contains(QStringLiteral("exit 2")));
    QVERIFY(f.rootEntries().isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_proton_install_job)
#include "tst_proton_install_job.moc"
