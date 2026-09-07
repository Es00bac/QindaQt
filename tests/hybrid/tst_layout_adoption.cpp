// SPDX-License-Identifier: GPL-3.0-or-later
#include "testfixtures.h"
#include <QtTest>
using namespace QindaQt;
using namespace QindaQt::Hybrid;
using namespace QindaQt::Hybrid::Test;
namespace {
struct Record {
  int prepared = 0;
  int rolledBack = 0;
  bool failPrepare = false;
  bool failCommit = false;
  bool oldStateVisible = true;
};
class Transaction final : public SceneTransaction {
public:
  Transaction(Record &record, const TopologyRepository &repository)
      : r(record), repo(repository) {}
  SceneStepResult prepare(const WindowTopology &before,
                          const WindowTopology &candidate,
                          const TopologyCommand &) override {
    ++r.prepared;
    r.oldStateVisible = repo.topology().revision() == before.revision() &&
                        repo.topology().isIndependent(QStringLiteral("a")) &&
                        candidate.ownerOf(QStringLiteral("a")).has_value();
    return r.failPrepare
               ? SceneStepResult::failure(QStringLiteral("prepare failure"))
               : SceneStepResult::ready();
  }
  SceneStepResult commit() override {
    r.oldStateVisible =
        r.oldStateVisible && repo.topology().isIndependent(QStringLiteral("a"));
    return r.failCommit
               ? SceneStepResult::failure(QStringLiteral("commit failure"))
               : SceneStepResult::ready();
  }
  void rollback() noexcept override { ++r.rolledBack; }

private:
  Record &r;
  const TopologyRepository &repo;
};
class Factory final : public SceneTransactionFactory {
public:
  Factory(Record &record, const TopologyRepository &repository)
      : r(record), repo(repository) {}
  std::unique_ptr<SceneTransaction> create() override {
    return std::make_unique<Transaction>(r, repo);
  }

private:
  Record &r;
  const TopologyRepository &repo;
};
Core::WindowContainer layout() {
  auto result =
      splitContainer(QStringLiteral("restored"), QStringLiteral("layout"),
                     QStringLiteral("a"), QStringLiteral("b"));
  if (!result.addPage(QStringLiteral("notes"), QStringLiteral("note-leaf"),
                      QStringLiteral("c")) ||
      !result.activatePage(QStringLiteral("notes")))
    qFatal("Invalid fixture");
  return result;
}
} // namespace
class AdoptionTest final : public QObject {
  Q_OBJECT
private slots:
  void publishesWholeLayoutOnce() {
    TopologyRepository repository(
        topology({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
                  QStringLiteral("other")},
                 {}, 7));
    Record record;
    Factory factory(record, repository);
    TopologyCoordinator coordinator(repository, factory);
    const auto expected = layout();
    const auto result = coordinator.execute(AdoptIndependentLayout{expected});
    QVERIFY2(result.committed(), qPrintable(result.message));
    QCOMPARE(result.kind, TopologyCommandKind::AdoptIndependentLayout);
    QCOMPARE(repository.topology().revision(), quint64(8));
    QCOMPARE(repository.topology().containerIds(),
             QStringList{QStringLiteral("restored")});
    const auto *restored =
        repository.topology().container(QStringLiteral("restored"));
    QVERIFY(restored);
    QCOMPARE(restored->toJson(), expected.toJson());
    QVERIFY(repository.topology().isIndependent(QStringLiteral("other")));
    QVERIFY(!repository.topology().isIndependent(QStringLiteral("a")));
    QVERIFY(record.oldStateVisible);
    QCOMPARE(record.prepared, 1);
  }
  void rejectsClosedOrAlreadyGroupedMember_data() {
    QTest::addColumn<bool>("grouped");
    QTest::newRow("closed") << false;
    QTest::newRow("already-grouped") << true;
  }
  void rejectsClosedOrAlreadyGroupedMember() {
    QFETCH(bool, grouped);
    QVector<Core::WindowContainer> containers;
    if (grouped)
      containers.append(
          splitContainer(QStringLiteral("existing"), QStringLiteral("old"),
                         QStringLiteral("c"), QStringLiteral("d")));
    TopologyRepository repository(
        topology({QStringLiteral("a"), QStringLiteral("b")}, containers, 7));
    Record record;
    Factory factory(record, repository);
    TopologyCoordinator coordinator(repository, factory);
    const auto result = coordinator.execute(AdoptIndependentLayout{layout()});
    QCOMPARE(result.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64(7));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("a")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("b")));
    QCOMPARE(record.prepared, 0);
  }
  void sceneFailurePreservesOwnership_data() {
    QTest::addColumn<bool>("prepareFailure");
    QTest::newRow("prepare") << true;
    QTest::newRow("commit") << false;
  }
  void sceneFailurePreservesOwnership() {
    QFETCH(bool, prepareFailure);
    TopologyRepository repository(topology(
        {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}, {},
        7));
    Record record;
    record.failPrepare = prepareFailure;
    record.failCommit = !prepareFailure;
    Factory factory(record, repository);
    TopologyCoordinator coordinator(repository, factory);
    const auto result = coordinator.execute(AdoptIndependentLayout{layout()});
    QVERIFY(!result.committed());
    QCOMPARE(repository.topology().revision(), quint64(7));
    QVERIFY(repository.topology().containerIds().isEmpty());
    QVERIFY(repository.topology().isIndependent(QStringLiteral("a")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("b")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("c")));
    QCOMPARE(record.rolledBack, 1);
    QVERIFY(record.oldStateVisible);
  }
};
QTEST_GUILESS_MAIN(AdoptionTest)
#include "tst_layout_adoption.moc"
