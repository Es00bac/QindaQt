// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/mutation_controller.h"

#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {
QVariantMap entry(const QString &path) {
  return {{QStringLiteral("path"), path},
          {QStringLiteral("device"), QStringLiteral("1")},
          {QStringLiteral("inode"), QStringLiteral("2")},
          {QStringLiteral("identitySize"), QStringLiteral("3")},
          {QStringLiteral("modifiedNanoseconds"), QStringLiteral("4")},
          {QStringLiteral("mode"), QStringLiteral("5")}};
}
class OutputBackend final : public MutationBackend {
public:
  QVector<MutationResult> replies;
  std::atomic_int calls = 0;
  bool cancelAfterFirst = false;
  MutationResult execute(const MutationRequest &, const MutationCancellation &cancel,
                         const MutationProgressCallback &) override {
    const int index = calls.fetch_add(1);
    if (cancelAfterFirst && index == 0)
      cancel->store(true);
    return replies.at(index);
  }
};
MutationResult partial(MutationOutputDisposition disposition, const QString &path) {
  MutationResult result;
  result.error = MutationError::Cancelled;
  result.diagnostic = QStringLiteral("Cancelled");
  result.outputObservation.disposition = disposition;
  result.outputObservation.path = path;
  result.outputObservation.exclusiveCreation = true;
  return result;
}
} // namespace

class MutationOutputTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void failureOutputIsVisibleWithoutGrantingRecovery_data() {
    QTest::addColumn<int>("disposition");
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("notice");
    QTest::newRow("owned-partial") << int(MutationOutputDisposition::RetainedPartial)
        << QStringLiteral("retained-partial") << QStringLiteral("Partial copy retained");
    QTest::newRow("completed-output") << int(MutationOutputDisposition::RetainedCopy)
        << QStringLiteral("retained-copy") << QStringLiteral("Copy output retained");
    QTest::newRow("foreign-replacement") << int(MutationOutputDisposition::Replaced)
        << QStringLiteral("replaced") << QStringLiteral("Destination changed");
    QTest::newRow("unconfirmed") << int(MutationOutputDisposition::Unconfirmed)
        << QStringLiteral("unconfirmed") << QStringLiteral("could not be confirmed");
  }
  void failureOutputIsVisibleWithoutGrantingRecovery() {
    QFETCH(int, disposition);
    QFETCH(QString, key);
    QFETCH(QString, notice);
    const QString path = QStringLiteral("/fixture/<b>literal & output</b>");
    auto backend = std::make_unique<OutputBackend>();
    backend->replies = {partial(static_cast<MutationOutputDisposition>(disposition), path), {}};
    MutationController controller(std::move(backend));
    QSignalSpy refresh(&controller, &MutationController::mutationCommitted);
    QVERIFY(controller.copyItem(QStringLiteral("/source/file"), path, entry(QString())));
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.failureCode(), QStringLiteral("cancelled"));
    QVERIFY(controller.resultText().isEmpty());
    QVERIFY(controller.outputNotice().contains(notice));
    QVERIFY(controller.outputNotice().contains(path));
    QCOMPARE(controller.outputObservations().size(), 1);
    const auto output = controller.outputObservations().first().toMap();
    QCOMPARE(output.value(QStringLiteral("outputDisposition")).toString(), key);
    QCOMPARE(output.value(QStringLiteral("outputPath")).toString(), path);
    QCOMPARE(refresh.count(), 1);
    QVERIFY(!controller.canUndo());
    QVERIFY(!controller.canRestore());
    controller.clearFailure();
    QVERIFY(controller.outputNotice().isEmpty());
    QVERIFY(controller.copyItem(QStringLiteral("/source/next"), QStringLiteral("/out/next"), entry(QString())));
    QVERIFY(controller.outputNotice().isEmpty());
    QVERIFY(controller.outputObservations().isEmpty());
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.failureCode(), QStringLiteral("none"));
  }
  void batchPreservesSuccessFailureAndUnattemptedSuffix() {
    auto backend = std::make_unique<OutputBackend>();
    auto *recording = backend.get();
    backend->replies = {{}, partial(MutationOutputDisposition::Replaced, QStringLiteral("/out/b"))};
    MutationController controller(std::move(backend));
    QSignalSpy refresh(&controller, &MutationController::mutationCommitted);
    QVERIFY(controller.copyItemsTo({entry(QStringLiteral("/in/a")), entry(QStringLiteral("/in/b")),
                                    entry(QStringLiteral("/in/c"))}, QStringLiteral("/out")));
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(recording->calls.load(), 2);
    const auto results = controller.outputObservations();
    QCOMPARE(results.size(), 3);
    QVERIFY(results[0].toMap().value(QStringLiteral("attempted")).toBool());
    QCOMPARE(results[0].toMap().value(QStringLiteral("error")).toString(), QStringLiteral("none"));
    QVERIFY(results[1].toMap().value(QStringLiteral("attempted")).toBool());
    QCOMPARE(results[1].toMap().value(QStringLiteral("error")).toString(), QStringLiteral("cancelled"));
    QCOMPARE(results[1].toMap().value(QStringLiteral("outputDisposition")).toString(), QStringLiteral("replaced"));
    QVERIFY(!results[2].toMap().value(QStringLiteral("attempted")).toBool());
    QCOMPARE(results[2].toMap().value(QStringLiteral("sourcePath")).toString(), QStringLiteral("/in/c"));
    QVERIFY(controller.outputNotice().contains(QStringLiteral("1 earlier items completed")));
    QVERIFY(controller.outputNotice().contains(QStringLiteral("/out/b")));
    QCOMPARE(refresh.count(), 1);
  }
  void betweenItemCancellationDoesNotRelabelThePreviousOutput() {
    auto backend = std::make_unique<OutputBackend>();
    auto *recording = backend.get();
    backend->cancelAfterFirst = true;
    MutationResult success;
    success.outputObservation.disposition = MutationOutputDisposition::RetainedCopy;
    success.outputObservation.path = QStringLiteral("/out/a");
    backend->replies = {success};
    MutationController controller(std::move(backend));
    QSignalSpy refresh(&controller, &MutationController::mutationCommitted);
    QVERIFY(controller.copyItemsTo({entry(QStringLiteral("/in/a")), entry(QStringLiteral("/in/b"))},
                                   QStringLiteral("/out")));
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(recording->calls.load(), 1);
    QCOMPARE(controller.failureCode(), QStringLiteral("cancelled"));
    QCOMPARE(controller.outputObservations().size(), 2);
    QCOMPARE(controller.outputObservations()[0].toMap().value(QStringLiteral("error")).toString(),
             QStringLiteral("none"));
    QVERIFY(!controller.outputObservations()[1].toMap().value(QStringLiteral("attempted")).toBool());
    QVERIFY(!controller.outputNotice().contains(QStringLiteral("failed: /out/a")));
    QVERIFY(controller.outputNotice().contains(QStringLiteral("1 earlier items completed")));
    QCOMPARE(refresh.count(), 1);
  }
  void failedExclusiveCreationDoesNotClaimOutput() {
    auto backend = std::make_unique<OutputBackend>();
    MutationResult collision;
    collision.error = MutationError::AlreadyExists;
    collision.diagnostic = QStringLiteral("Already exists");
    backend->replies = {collision};
    MutationController controller(std::move(backend));
    QSignalSpy refresh(&controller, &MutationController::mutationCommitted);
    QVERIFY(controller.copyItem(QStringLiteral("/in/file"), QStringLiteral("/out/file"), entry(QString())));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.outputNotice().isEmpty());
    QCOMPARE(refresh.count(), 0);
    QCOMPARE(controller.outputObservations()[0].toMap().value(QStringLiteral("outputDisposition")).toString(),
             QStringLiteral("none"));
  }
};

QTEST_GUILESS_MAIN(MutationOutputTests)
#include "tst_mutation_output.moc"
