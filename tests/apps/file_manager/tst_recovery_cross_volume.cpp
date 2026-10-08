// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_test_support.h"
#include <QtTest>
#include <QUuid>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace RecoveryTest;
class CrossVolumeRecoveryTests final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    TwoVolumes v; QVERIFY2(v.valid(), "Actual distinct st_dev temporary fixtures are required, never fake IDs.");
    const auto loaded = reinterpret_cast<int (*)()>(::dlsym(RTLD_DEFAULT, "qindaqt_ed05_fault_loaded"));
    QVERIFY2(loaded && loaded() == 1, "Stage-scoped I/O failure shim must actually be loaded.");
  }
  void completedRetentionAndFreshRestorePreserveDestination() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "source-bytes"));
    LocalMutationBackend backend(QDir(v.source.path()).filePath(QStringLiteral("trash")), std::make_shared<LocalDeviceResolver>(), nullptr, v.catalog);
    const auto moved = backend.execute(v.request(), {}, {});
    QVERIFY2(moved.ok(), qPrintable(moved.diagnostic));
    QVERIFY(!QFileInfo::exists(v.sourcePath)); QCOMPARE(read(v.destinationPath), QByteArray("source-bytes"));
    QCOMPARE(moved.recovery.disposition, MutationRecoveryDisposition::CompletedWithRetention);
    QCOMPARE(read(payload(moved.recovery.recoveryDirectory)), QByteArray("source-bytes"));
    QVERIFY(moved.recovery.restoreAvailable); QVERIFY(moved.undoRequest);
    const auto restored = backend.execute(*moved.undoRequest, {}, {});
    QVERIFY2(restored.ok(), qPrintable(restored.diagnostic));
    QCOMPARE(read(v.sourcePath), QByteArray("source-bytes")); QCOMPARE(read(v.destinationPath), QByteArray("source-bytes"));
  }
  void cancellationAtEveryPhase_data() {
    QTest::addColumn<int>("phase");
    for (const auto phase : {CrossVolumeStep::Prepared, CrossVolumeStep::Copy, CrossVolumeStep::Verified,
                            CrossVolumeStep::BeforePublish, CrossVolumeStep::AfterPublish,
                            CrossVolumeStep::BeforeRetirement, CrossVolumeStep::AfterRetirement,
                            CrossVolumeStep::FinalVerification})
      QTest::newRow(qPrintable(QString::number(static_cast<int>(phase)))) << static_cast<int>(phase);
  }
  void cancellationAtEveryPhase() {
    QFETCH(int, phase); TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "must-survive"));
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (static_cast<int>(step) == phase) cancel->store(true);
      return MutationError::None;
    });
    const auto result = mover.move(v.request(), cancel, {});
    QCOMPARE(result.error, MutationError::Cancelled);
    const auto held = privatePath(v.sourceDirectory, QStringLiteral(".qindaqt-recovery-"));
    QVERIFY(read(v.sourcePath) == QByteArray("must-survive") || read(payload(held)) == QByteArray("must-survive"));
    if (QFileInfo::exists(v.destinationPath)) QCOMPARE(read(v.destinationPath), QByteArray("must-survive"));
  }
  void durabilityFailureAtEveryPhase_data() {
    QTest::addColumn<int>("phase");
    for (const auto phase : {CrossVolumeStep::Prepared, CrossVolumeStep::Copy, CrossVolumeStep::Verified,
                            CrossVolumeStep::BeforePublish, CrossVolumeStep::AfterPublish,
                            CrossVolumeStep::SyncDestination, CrossVolumeStep::BeforeRetirement,
                            CrossVolumeStep::AfterRetirement, CrossVolumeStep::SyncRecovery,
                            CrossVolumeStep::SyncSource, CrossVolumeStep::FinalVerification})
      QTest::newRow(qPrintable(QString::number(static_cast<int>(phase)))) << static_cast<int>(phase);
  }
  void durabilityFailureAtEveryPhase() {
    QFETCH(int, phase); TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "preserve-all"));
    CrossVolumeMove mover(v.catalog, [phase](CrossVolumeStep step) {
      return static_cast<int>(step) == phase ? MutationError::DiskFull : MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {});
    QCOMPARE(result.error, MutationError::DiskFull);
    const auto held = privatePath(v.sourceDirectory, QStringLiteral(".qindaqt-recovery-"));
    QVERIFY(read(v.sourcePath) == QByteArray("preserve-all") || read(payload(held)) == QByteArray("preserve-all"));
  }
  void everyJournalBarrierPreservesSource_data() {
    QTest::addColumn<int>("sequence"); QTest::addColumn<int>("barrier");
    for (int sequence = 0; sequence < 6; ++sequence)
      for (int barrier = 0; barrier < 5; ++barrier)
        QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(sequence).arg(barrier))) << sequence << barrier;
  }
  void everyJournalBarrierPreservesSource() {
    QFETCH(int, sequence); QFETCH(int, barrier);
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "journal-source"));
    int current = -1;
    CrossVolumeMove mover(v.catalog, {}, [&](RecoveryStoreStep step) {
      if (step == RecoveryStoreStep::CreateRecord) ++current;
      return current == sequence && static_cast<int>(step) == barrier ? MutationError::DiskFull : MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {});
    QCOMPARE(result.error, MutationError::DiskFull);
    const auto held = privatePath(v.sourceDirectory, QStringLiteral(".qindaqt-recovery-"));
    QVERIFY(read(v.sourcePath) == QByteArray("journal-source") || read(payload(held)) == QByteArray("journal-source"));
  }
  void realWriteAndMetadataFailures_data() {
    QTest::addColumn<QByteArray>("call"); QTest::addColumn<int>("error");
    QTest::newRow("write-enospc") << QByteArray("write") << static_cast<int>(MutationError::DiskFull);
    QTest::newRow("file-fsync-enospc") << QByteArray("fsync") << static_cast<int>(MutationError::DiskFull);
    QTest::newRow("permissions") << QByteArray("fchmod") << static_cast<int>(MutationError::PermissionDenied);
    QTest::newRow("times") << QByteArray("futimens") << static_cast<int>(MutationError::PermissionDenied);
  }
  void realWriteAndMetadataFailures() {
    QFETCH(QByteArray, call); QFETCH(int, error);
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "source"));
    qputenv("QINDAQT_ED05_FAIL_CALL", call);
    CrossVolumeMove mover(v.catalog); const auto result = mover.move(v.request(), {}, {});
    qunsetenv("QINDAQT_ED05_FAIL_CALL");
    QCOMPARE(result.error, static_cast<MutationError>(error)); QCOMPARE(read(v.sourcePath), QByteArray("source"));
    QVERIFY(!QFileInfo::exists(v.destinationPath));
  }
  void catalogCapacityAndMalformedLocatorNeverEvictRetainedData() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "catalog-source"));
    CrossVolumeMove mover(v.catalog); const auto moved = mover.move(v.request(), {}, {}); QVERIFY(moved.ok());
    {
      RecoveryCatalog catalog(v.catalog); QVERIFY(catalog.admit(false).ok());
      const auto initial = catalog.inspect(); QVERIFY(initial.result.ok()); QCOMPARE(initial.records.size(), qsizetype(1));
      // Non-authoritative value locators qualify index bounds, never transfers.
      for (quint32 index = 1; index < maximumRecoveryOperations; ++index) {
        auto locator = initial.records.first();
        locator.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(catalog.add(locator).ok());
      }
      auto overflow = initial.records.first(); overflow.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
      QCOMPARE(catalog.add(overflow).error, MutationError::Unsupported);
      QCOMPARE(catalog.inspect().records.size(), static_cast<qsizetype>(maximumRecoveryOperations));
    }
    const auto held = payload(moved.recovery.recoveryDirectory);
    QCOMPARE(read(held), QByteArray("catalog-source"));
    const auto corrupted = QDir(v.catalog).filePath(moved.recovery.operationId + QStringLiteral(".json"));
    QVERIFY(write(corrupted, "malformed-locator"));
    const auto inspected = mover.inspect({}); QVERIFY(!inspected.ok());
    QCOMPARE(read(corrupted), QByteArray("malformed-locator"));
    QCOMPARE(read(held), QByteArray("catalog-source")); QCOMPARE(read(v.destinationPath), QByteArray("catalog-source"));
  }
  void restoreCollisionAndSurvivingWriter() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    const int held = ::open(QFile::encodeName(v.sourcePath).constData(), O_WRONLY | O_CLOEXEC);
    QVERIFY(held >= 0);
    CrossVolumeMove mover(v.catalog); const auto moved = mover.move(v.request(), {}, {});
    QVERIFY2(moved.ok(), qPrintable(moved.diagnostic));
    QCOMPARE(::pwrite(held, "later", 5, 0), ssize_t(5)); ::close(held);
    QVERIFY(write(v.sourcePath, "foreign-collision"));
    QCOMPARE(mover.restore(moved.recovery.operationId).error, MutationError::AlreadyExists);
    QCOMPARE(read(v.sourcePath), QByteArray("foreign-collision"));
    QCOMPARE(read(v.destinationPath), QByteArray("original"));
    // Test-owned collision is removed deliberately; production restore has no
    // such deletion. Current retained contents, not the old digest, restore.
    QVERIFY(QFile::remove(v.sourcePath));
    const auto restored = mover.restore(moved.recovery.operationId);
    QVERIFY2(restored.ok(), qPrintable(restored.diagnostic));
    QCOMPARE(read(v.sourcePath), QByteArray("laternal")); QCOMPARE(read(v.destinationPath), QByteArray("original"));
  }
  void processExitThenRestartInspectionNeverReplays_data() {
    QTest::addColumn<int>("phase");
    for (const auto phase : {CrossVolumeStep::Prepared, CrossVolumeStep::Copy,
                            CrossVolumeStep::AfterPublish, CrossVolumeStep::AfterRetirement})
      QTest::newRow(qPrintable(QString::number(static_cast<int>(phase)))) << static_cast<int>(phase);
  }
  void processExitThenRestartInspectionNeverReplays() {
    QFETCH(int, phase); TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "restart-source"));
    const auto request = v.request();
    const pid_t child = ::fork(); QVERIFY(child >= 0);
    if (child == 0) {
      CrossVolumeMove mover(v.catalog, [phase](CrossVolumeStep step) {
        if (static_cast<int>(step) == phase) ::_exit(42);
        return MutationError::None;
      });
      const auto unused = mover.move(request, {}, {}); Q_UNUSED(unused); ::_exit(43);
    }
    int exitStatus = 0; QCOMPARE(::waitpid(child, &exitStatus, 0), child);
    QVERIFY(WIFEXITED(exitStatus)); QCOMPARE(WEXITSTATUS(exitStatus), 42);
    const auto originalBefore = read(v.sourcePath), destinationBefore = read(v.destinationPath);
    const auto recovery = privatePath(v.sourceDirectory, QStringLiteral(".qindaqt-recovery-"));
    const auto retainedBefore = read(payload(recovery));
    CrossVolumeMove fresh(v.catalog);
    const auto inspected = fresh.inspect({}); QVERIFY(inspected.ok());
    QVERIFY(!inspected.recoveryReceipts.isEmpty());
    QCOMPARE(read(v.sourcePath), originalBefore); QCOMPARE(read(v.destinationPath), destinationBefore);
    QCOMPARE(read(payload(recovery)), retainedBefore);
    if (phase == static_cast<int>(CrossVolumeStep::AfterRetirement)) {
      const auto restored = fresh.restore(inspected.recovery.operationId);
      QVERIFY2(restored.ok(), qPrintable(restored.diagnostic)); QCOMPARE(read(v.sourcePath), QByteArray("restart-source"));
      QCOMPARE(read(v.destinationPath), QByteArray("restart-source"));
    }
  }
};
QTEST_GUILESS_MAIN(CrossVolumeRecoveryTests)
#include "tst_recovery_cross_volume.moc"
