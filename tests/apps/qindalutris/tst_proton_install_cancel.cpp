// SPDX-License-Identifier: GPL-3.0-or-later
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTest>

#include "proton_job_fixture.h"

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

// Cancelling ProtonInstallJob in every stage: nothing is left under the
// root, finished(cancelled) is queued, and when a tar process runs it is
// dead (with its children) before staging is removed.
namespace {

QString marker() {
  return QStringLiteral("4%1.%2")
      .arg(QRandomGenerator::global()->bounded(100, 999))
      .arg(QRandomGenerator::global()->bounded(1000, 9999));
}

int processesWith(const QString &needle) {
  int count = 0;
  const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString &entry : entries) {
    QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(entry));
    if (cmdline.open(QIODevice::ReadOnly) && cmdline.readAll().contains(needle.toUtf8())) {
      ++count;
    }
  }
  return count;
}

// Cancels as soon as the job reports a stage starting with `stagePrefix`.
void cancelAt(ProtonFixture &f, const QString &stagePrefix) {
  QObject::connect(&f.job, &ProtonInstallJob::progress, &f.job,
                   [&f, stagePrefix](double, const QString &text) {
                     if (text.startsWith(stagePrefix)) {
                       f.job.cancel();
                     }
                   });
}

void verifyCancelledClean(ProtonFixture &f) {
  QVERIFY(f.wait());
  QVERIFY(f.result->cancelled);
  QVERIFY(!f.result->ok);
  QVERIFY(!f.job.isRunning());
  QVERIFY2(f.rootEntries().isEmpty(),
           qPrintable(f.rootEntries().join(QLatin1Char(' ')) + QLatin1Char('\n') + f.job.detailsText()));
}

} // namespace

class tst_proton_install_cancel : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() {
    if (QStandardPaths::findExecutable(QStringLiteral("tar")).isEmpty() ||
        QStandardPaths::findExecutable(QStringLiteral("sleep")).isEmpty()) {
      QSKIP("tar/sleep are not installed on this host");
    }
  }

  void cancelDuringDownload() {
    ProtonFixture f;
    f.downloader.script(testRelease().tarballUrl, FakeDownloader::Mode::Hang);
    f.job.start(testRelease());
    QVERIFY(f.rootEntries().first().startsWith(QStringLiteral(".qindalutris-staging-")));
    f.job.cancel();
    QVERIFY(f.job.isRunning()); // until the queued result is delivered
    verifyCancelledClean(f);
    QCOMPARE(f.downloader.cancels, 1);
  }

  void cancelDuringHashing() {
    ProtonFixture f;
    f.serveBuild();
    cancelAt(f, QStringLiteral("Checking the download"));
    f.job.start(testRelease());
    verifyCancelledClean(f);
    QVERIFY(!f.job.detailsText().contains(QStringLiteral("SHA-512 verified")));
  }

  void cancelDuringListingStopsTar() {
    ProtonFixture f;
    const QString mark = marker();
    f.job.setTarProgram(writeScript(f.dir.filePath(QStringLiteral("slow-tar")),
                                    "sleep " + mark.toUtf8() + " & sleep " + mark.toUtf8() + "\n"));
    f.serveBuild();
    f.job.start(testRelease());
    QVERIFY(QTest::qWaitFor([&] { return processesWith(mark) >= 2; }, 10000));
    f.job.cancel();
    verifyCancelledClean(f);
    QCOMPARE(processesWith(mark), 0);
  }

  void cancelDuringUnpackingStopsTarBeforeStagingIsRemoved() {
    ProtonFixture f;
    const QString mark = marker();
    const QString realTar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    // A tar that lists for real but "unpacks" forever, with a child.
    f.job.setTarProgram(writeScript(
        f.dir.filePath(QStringLiteral("slow-extract-tar")),
        "for a in \"$@\"; do if [ \"$a\" = --extract ]; then sleep " + mark.toUtf8() + " & sleep " +
            mark.toUtf8() + "; fi; done\nexec " + realTar.toUtf8() + " \"$@\"\n"));
    f.serveBuild();
    f.job.start(testRelease());
    QVERIFY(QTest::qWaitFor([&] { return processesWith(mark) >= 2; }, 10000));
    QVERIFY(f.job.detailsText().contains(QStringLiteral("Archive listing accepted")));
    f.job.cancel();
    QVERIFY(!f.result.has_value());
    verifyCancelledClean(f);
    QCOMPARE(processesWith(mark), 0);
  }

  void cancelWhenIdleIsANoOp() {
    ProtonFixture f;
    f.job.cancel();
    QTest::qWait(20);
    QVERIFY(!f.result.has_value());
  }
};

QTEST_GUILESS_MAIN(tst_proton_install_cancel)
#include "tst_proton_install_cancel.moc"
