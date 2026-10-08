// SPDX-License-Identifier: GPL-3.0-or-later
#include "trash_test_support.h"
#include "mutation/mutation_controller.h"
#include <QSignalSpy>
#include <QTest>
using namespace TrashTest;
namespace {
class CancelAfterOne final : public MutationBackend {
public:
  explicit CancelAfterOne(VolumeTrash owner) : m_owner(std::move(owner)) {}
  MutationResult execute(const MutationRequest &request, const MutationCancellation &cancel,
                         const MutationProgressCallback &) override {
    auto result = m_owner.trash(request, cancel);
    if (result.ok()) cancel->store(true);
    return result;
  }
private:
  VolumeTrash m_owner;
};
std::unique_ptr<LocalMutationBackend> backend(const Fixture &f) {
  return std::make_unique<LocalMutationBackend>(f.homeRoot(),
      std::make_shared<LocalDeviceResolver>(), nullptr, f.home.filePath("recovery"), f.top);
}
}
class TrashControllerTests final : public QObject {
  Q_OBJECT
private slots:
  void restoreLastRetainsVolumeAndChosenFolder() {
    Fixture f; QVERIFY(f.admit()); const auto original = f.source("item<b>&%");
    QVERIFY(write(original, "payload"));
    const auto identity = LocalMutationBackend::identityForPath(original); QVERIFY(identity);
    MutationController controller(backend(f));
    QVERIFY(controller.trashItem(original, identityMap(*identity, original)));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.failureCode(), QStringLiteral("none")); QVERIFY(controller.canRestore());
    QCOMPARE(controller.lastTrashOriginalPath(), original);
    QVERIFY(QDir().rmdir(f.sourceFolder()));
    QVERIFY(!controller.restoreLast()); QCOMPARE(controller.failureCode(), QStringLiteral("vanished"));
    const auto chosen = f.volume.filePath("chosen"); QVERIFY(privateDir(chosen));
    QVERIFY(controller.restoreLastTo(chosen)); QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.failureCode(), QStringLiteral("none"));
    QCOMPARE(read(QDir(chosen).filePath("item<b>&%")), QByteArray("payload")); QVERIFY(!controller.canRestore());
    QVERIFY(controller.outputNotice().contains("item<b>&%"));
  }
  void putBackChosenFolderAfterRestart() {
    Fixture f; QVERIFY(f.admit()); QVERIFY(write(f.source(), "payload"));
    const auto saved = f.owner().trash(f.trash(f.source()), f.cancel); QVERIFY(saved.ok());
    QVERIFY(QDir().rmdir(f.sourceFolder()));
    MutationController restarted(backend(f));
    const QVariantList items{identityMap(*saved.outputIdentity, saved.outputPath)};
    QCOMPARE(restarted.originalTrashPath(saved.outputPath), saved.originalPath);
    QVERIFY(!restarted.putBackItems(items));
    const auto chosen = f.volume.filePath("chosen"); QVERIFY(privateDir(chosen));
    QVERIFY(restarted.putBackItemsTo(items, chosen)); QTRY_VERIFY_WITH_TIMEOUT(!restarted.busy(), 3000);
    QCOMPARE(restarted.failureCode(), QStringLiteral("none")); QCOMPARE(read(QDir(chosen).filePath("item")), QByteArray("payload"));
  }
  void partialBatchPreservesReceiptsAndUnattemptedSuffix() {
    Fixture f; QVERIFY(f.admit()); QVariantList items;
    for (const auto &name : {"first", "second", "third"}) {
      const auto path = f.source(QString::fromLatin1(name)); QVERIFY(write(path, name));
      const auto identity = LocalMutationBackend::identityForPath(path); QVERIFY(identity);
      items.append(identityMap(*identity, path));
    }
    QVERIFY(write(f.source("second"), "changed-second"));
    MutationController controller(backend(f)); QSignalSpy committed(&controller, &MutationController::mutationCommitted);
    QVERIFY(controller.trashItems(items)); QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.failureCode(), QStringLiteral("changed")); QCOMPARE(committed.size(), 1);
    const auto outcomes = controller.outputObservations(); QCOMPARE(outcomes.size(), 3);
    QVERIFY(outcomes[0].toMap()["attempted"].toBool()); QCOMPARE(outcomes[0].toMap()["error"].toString(), QStringLiteral("none"));
    QVERIFY(!outcomes[0].toMap()["trash"].toMap()["payloadPath"].toString().isEmpty());
    QVERIFY(outcomes[1].toMap()["attempted"].toBool()); QVERIFY(!outcomes[2].toMap()["attempted"].toBool());
    QVERIFY(!QFileInfo::exists(f.source("first"))); QCOMPARE(read(f.source("second")), QByteArray("changed-second"));
    QCOMPARE(read(f.source("third")), QByteArray("third")); QVERIFY(controller.outputNotice().contains("1 earlier items completed"));
    QVERIFY(controller.outputNotice().contains(f.privateRoot()));
  }
  void betweenItemsCancellationKeepsSuccessfulPrefix() {
    Fixture f; QVERIFY(f.admit()); QVariantList items;
    for (const auto &name : {"first", "second"}) {
      const auto path = f.source(QString::fromLatin1(name)); QVERIFY(write(path, name));
      const auto identity = LocalMutationBackend::identityForPath(path); QVERIFY(identity);
      items.append(identityMap(*identity, path));
    }
    MutationController controller(std::make_unique<CancelAfterOne>(f.owner()));
    QVERIFY(controller.trashItems(items)); QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.failureCode(), QStringLiteral("cancelled"));
    const auto outcomes = controller.outputObservations(); QCOMPARE(outcomes.size(), 2);
    QVERIFY(outcomes[0].toMap()["attempted"].toBool()); QVERIFY(!outcomes[1].toMap()["attempted"].toBool());
    QVERIFY(!QFileInfo::exists(f.source("first"))); QCOMPARE(read(f.source("second")), QByteArray("second"));
    QVERIFY(controller.outputNotice().contains("first"));
  }
};
QTEST_GUILESS_MAIN(TrashControllerTests)
#include "tst_trash_controller.moc"
