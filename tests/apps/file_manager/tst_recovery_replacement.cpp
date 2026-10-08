// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_test_support.h"
#include <QtTest>
#include <fcntl.h>
#include <unistd.h>
using namespace RecoveryTest;
class RecoveryReplacementTests final : public QObject {
  Q_OBJECT
private slots:
  void publicationWindowPreservesForeignAndCopiedEntries() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    QString saved;
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (step == CrossVolumeStep::PublicationWindow) {
        const auto stage = privatePath(v.destinationDirectory, QStringLiteral(".qindaqt-stage-"));
        saved = QDir(stage).filePath(QStringLiteral("saved-copy"));
        if (!QFile::rename(payload(stage), saved) || !write(payload(stage), "foreign")) return MutationError::IoError;
      }
      return MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {});
    QCOMPARE(result.error, MutationError::Changed);
    QCOMPARE(read(v.sourcePath), QByteArray("original")); QCOMPARE(read(saved), QByteArray("original"));
    QCOMPARE(read(v.destinationPath), QByteArray("foreign"));
  }
  void retirementWindowNeverLabelsForeignCaptureOriginalRetention() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    const auto saved = QDir(v.sourceDirectory).filePath(QStringLiteral("saved-original"));
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (step == CrossVolumeStep::RetirementWindow &&
          (!QFile::rename(v.sourcePath, saved) || !write(v.sourcePath, "foreign")))
        return MutationError::IoError;
      return MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {});
    QCOMPARE(result.error, MutationError::Changed);
    QCOMPARE(result.recovery.disposition, MutationRecoveryDisposition::UnknownPlacement);
    QVERIFY(!result.recovery.restoreAvailable);
    QVERIFY(result.diagnostic.contains(QStringLiteral("returned")));
    QCOMPARE(read(saved), QByteArray("original")); QCOMPARE(read(v.destinationPath), QByteArray("original"));
    QCOMPARE(read(v.sourcePath), QByteArray("foreign"));
    QVERIFY(!QFileInfo::exists(payload(result.recovery.recoveryDirectory)));
  }
  void unexpectedReturnAdmissionFailures_data() {
    QTest::addColumn<QString>("failure");
    for (const auto &name : {"collision", "late-collision", "changed-capture", "parent-loss", "cancel", "journal", "durability"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }
  void unexpectedReturnAdmissionFailures() {
    QFETCH(QString, failure); TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    const auto saved = QDir(v.sourceDirectory).filePath(QStringLiteral("saved-original"));
    const auto disconnected = QDir(v.source.path()).filePath(QStringLiteral("lost-parent"));
    MutationCancellation cancel = std::make_shared<std::atomic_bool>(false);
    QString savedCapture;
    bool refuseRepairJournal = false;
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (step == CrossVolumeStep::RetirementWindow &&
          (!QFile::rename(v.sourcePath, saved) || !write(v.sourcePath, "foreign")))
        return MutationError::IoError;
      if (step == CrossVolumeStep::BeforeUnexpectedReturn) {
        refuseRepairJournal = failure == QStringLiteral("journal");
        if (failure == QStringLiteral("collision") && !write(v.sourcePath, "new-name")) return MutationError::IoError;
        if (failure == QStringLiteral("changed-capture")) {
          const auto directory = privatePath(v.sourceDirectory, QStringLiteral(".qindaqt-recovery-"));
          savedCapture = QDir(directory).filePath(QStringLiteral("saved-capture"));
          if (!QFile::rename(payload(directory), savedCapture) || !write(payload(directory), "new-capture")) return MutationError::IoError;
        }
        if (failure == QStringLiteral("parent-loss") &&
            (!QDir().rename(v.sourceDirectory, disconnected) || !QDir().mkdir(v.sourceDirectory)))
          return MutationError::IoError;
        if (failure == QStringLiteral("cancel")) cancel->store(true);
      }
      if (step == CrossVolumeStep::UnexpectedReturnWindow && failure == QStringLiteral("late-collision") &&
          !write(v.sourcePath, "new-name")) return MutationError::IoError;
      if (step == CrossVolumeStep::AfterUnexpectedReturn && failure == QStringLiteral("durability"))
        return MutationError::IoError;
      return MutationError::None;
    }, [&](RecoveryStoreStep step) {
      return refuseRepairJournal && step == RecoveryStoreStep::CreateRecord
          ? MutationError::DiskFull : MutationError::None;
    });
    const auto result = mover.move(v.request(), cancel, {});
    QCOMPARE(result.error, MutationError::Changed); QVERIFY(result.recovery.uncertain);
    QVERIFY(!result.recovery.restoreAvailable);
    QCOMPARE(read(v.destinationPath), QByteArray("original"));
    if (failure == QStringLiteral("durability")) {
      QCOMPARE(result.recovery.disposition, MutationRecoveryDisposition::UnknownPlacement);
      QCOMPARE(read(v.sourcePath), QByteArray("foreign"));
      QVERIFY(result.diagnostic.contains(QStringLiteral("durability failed")));
    } else if (failure == QStringLiteral("parent-loss")) {
      QCOMPARE(result.recovery.disposition, MutationRecoveryDisposition::UnknownPlacement);
      QCOMPARE(read(QDir(disconnected).filePath(QFileInfo(saved).fileName())), QByteArray("original"));
      const auto retainedDirectory = privatePath(disconnected, QStringLiteral(".qindaqt-recovery-"));
      QCOMPARE(read(payload(retainedDirectory)), QByteArray("foreign"));
    } else {
      QCOMPARE(result.recovery.disposition, MutationRecoveryDisposition::UnexpectedEntryRetained);
      QCOMPARE(read(saved), QByteArray("original"));
      QCOMPARE(read(payload(result.recovery.recoveryDirectory)),
          failure == QStringLiteral("changed-capture") ? QByteArray("new-capture") : QByteArray("foreign"));
      if (failure == QStringLiteral("changed-capture")) QCOMPARE(read(savedCapture), QByteArray("foreign"));
      if (failure.contains(QStringLiteral("collision"))) QCOMPARE(read(v.sourcePath), QByteArray("new-name"));
    }
  }
  void recreatedSourceNamePreventsPlainCompletion() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (step == CrossVolumeStep::FinalVerification && !write(v.sourcePath, "foreign"))
        return MutationError::IoError;
      return MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {});
    QCOMPARE(result.error, MutationError::AlreadyExists);
    QCOMPARE(result.recovery.disposition, MutationRecoveryDisposition::SourceRetained);
    QCOMPARE(read(v.sourcePath), QByteArray("foreign"));
    QCOMPARE(read(payload(result.recovery.recoveryDirectory)), QByteArray("original"));
    QCOMPARE(read(v.destinationPath), QByteArray("original"));
  }
  void destinationChangesBeforeAndAfterRetirement_data() {
    QTest::addColumn<int>("phase");
    QTest::newRow("before") << static_cast<int>(CrossVolumeStep::BeforeRetirement);
    QTest::newRow("after") << static_cast<int>(CrossVolumeStep::AfterRetirement);
  }
  void destinationChangesBeforeAndAfterRetirement() {
    QFETCH(int, phase); TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (static_cast<int>(step) == phase && !write(v.destinationPath, "changed")) return MutationError::IoError;
      return MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {});
    QCOMPARE(result.error, MutationError::Changed); QCOMPARE(read(v.destinationPath), QByteArray("changed"));
    if (phase == static_cast<int>(CrossVolumeStep::BeforeRetirement)) QCOMPARE(read(v.sourcePath), QByteArray("original"));
    else QCOMPARE(read(payload(result.recovery.recoveryDirectory)), QByteArray("original"));
  }
  void disconnectAndCombinedParentEntryReplacements() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    const auto savedParent = QDir(v.source.path()).filePath(QStringLiteral("disconnected-source"));
    const auto oldDestination = QDir(v.destination.path()).filePath(QStringLiteral("disconnected-destination"));
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (step != CrossVolumeStep::BeforeRetirement) return MutationError::None;
      if (!QDir().rename(v.sourceDirectory, savedParent) || !QDir().mkdir(v.sourceDirectory) ||
          !write(v.sourcePath, "foreign-source") || !QDir().rename(v.destinationDirectory, oldDestination) ||
          !QDir().mkdir(v.destinationDirectory) || !write(v.destinationPath, "foreign-destination"))
        return MutationError::IoError;
      return MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {});
    QCOMPARE(result.error, MutationError::Changed);
    QCOMPARE(read(QDir(savedParent).filePath(QFileInfo(v.sourcePath).fileName())), QByteArray("original"));
    QCOMPARE(read(v.sourcePath), QByteArray("foreign-source")); QCOMPARE(read(v.destinationPath), QByteArray("foreign-destination"));
    QCOMPARE(read(QDir(oldDestination).filePath(QFileInfo(v.destinationPath).fileName())), QByteArray("original"));
  }
  void changedRecoveryPlacementIsNotRetentionSuccess() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    QString saved;
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (step == CrossVolumeStep::AfterRetirement) {
        const auto recovery = privatePath(v.sourceDirectory, QStringLiteral(".qindaqt-recovery-"));
        saved = QDir(recovery).filePath(QStringLiteral("saved-original"));
        if (!QFile::rename(payload(recovery), saved) || !write(payload(recovery), "foreign")) return MutationError::IoError;
        return MutationError::IoError;
      }
      return MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {});
    QVERIFY(!result.ok()); QCOMPARE(result.recovery.disposition, MutationRecoveryDisposition::UnexpectedEntryRetained);
    QCOMPARE(read(saved), QByteArray("original")); QCOMPARE(read(payload(result.recovery.recoveryDirectory)), QByteArray("foreign"));
    QCOMPARE(read(v.destinationPath), QByteArray("original"));
  }
  void lateOpenedChildWriteIsPreserved_data() {
    QTest::addColumn<int>("phase");
    QTest::newRow("before-retirement") << static_cast<int>(CrossVolumeStep::BeforeRetirement);
    QTest::newRow("after-retirement") << static_cast<int>(CrossVolumeStep::AfterRetirement);
  }
  void lateOpenedChildWriteIsPreserved() {
    QFETCH(int, phase); TwoVolumes v; QVERIFY(v.valid()); QVERIFY(QDir().mkdir(v.sourcePath));
    const auto child = QDir(v.sourcePath).filePath(QStringLiteral("child")); QVERIFY(write(child, "original"));
    const int held = ::open(QFile::encodeName(child).constData(), O_WRONLY | O_CLOEXEC); QVERIFY(held >= 0);
    bool changed = false;
    CrossVolumeMove mover(v.catalog, [&](CrossVolumeStep step) {
      if (static_cast<int>(step) == phase) changed = ::pwrite(held, "late", 4, 0) == 4;
      return MutationError::None;
    });
    const auto result = mover.move(v.request(), {}, {}); ::close(held);
    QVERIFY(changed); QCOMPARE(result.error, MutationError::Changed);
    QCOMPARE(read(QDir(v.destinationPath).filePath(QStringLiteral("child"))), QByteArray("original"));
    const auto current = QFileInfo::exists(v.sourcePath) ? child :
        QDir(payload(result.recovery.recoveryDirectory)).filePath(QStringLiteral("child"));
    QCOMPARE(read(current), QByteArray("lateinal"));
  }
  void restoreWindowReplacementPreservesEveryCandidate() {
    TwoVolumes v; QVERIFY(v.valid()); QVERIFY(write(v.sourcePath, "original"));
    CrossVolumeMove first(v.catalog); const auto moved = first.move(v.request(), {}, {}); QVERIFY(moved.ok());
    const auto saved = QDir(moved.recovery.recoveryDirectory).filePath(QStringLiteral("saved-original"));
    CrossVolumeMove restorer(v.catalog, [&](CrossVolumeStep step) {
      if (step == CrossVolumeStep::RestoreWindow &&
          (!QFile::rename(payload(moved.recovery.recoveryDirectory), saved) ||
           !write(payload(moved.recovery.recoveryDirectory), "foreign")))
        return MutationError::IoError;
      return MutationError::None;
    });
    const auto result = restorer.restore(moved.recovery.operationId);
    QCOMPARE(result.error, MutationError::Changed); QCOMPARE(read(saved), QByteArray("original"));
    QCOMPARE(read(v.sourcePath), QByteArray("foreign")); QCOMPARE(read(v.destinationPath), QByteArray("original"));
  }
};
QTEST_GUILESS_MAIN(RecoveryReplacementTests)
#include "tst_recovery_replacement.moc"
