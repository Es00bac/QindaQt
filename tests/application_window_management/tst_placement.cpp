// SPDX-License-Identifier: GPL-3.0-or-later
#include "../hybrid/testfixtures.h"
#include <QtTest>
#include <qindaqt/application_window_management/placement.h>
#include <qindaqt/hybrid/topologycoordinator.h>
using namespace QindaQt;
using namespace ApplicationWindowManagement;
class PlacementTests : public QObject {
  Q_OBJECT
private slots:
  void soloTabCreatesAndActivatesAtomically() {
    Hybrid::TopologyRepository repository(
        Hybrid::Test::topology({"source", "new"}));
    Hybrid::Test::AlwaysReadyFactory scene;
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    const auto plan = planPlacement(repository.topology(), "source", "new",
                                    Placement::Tab, "one");
    QVERIFY(plan.command);
    const auto result = coordinator.execute(*plan.command);
    QVERIFY(result.committed());
    const auto *container = repository.topology().container(plan.containerId);
    QVERIFY(container);
    QCOMPARE(container->pages().size(), 2);
    QCOMPARE(container->activePageId(), container->pages().at(1).id());
    QCOMPARE(repository.topology().revision(), quint64(1));
    QVERIFY(repository.topology().validate().valid);
    auto roundtrip = Core::WindowContainer::fromJson(container->toJson());
    QVERIFY(roundtrip);
    QCOMPARE(roundtrip->activePageId(), container->activePageId());
  }
  void existingMixedContainerAppendsAndSplitsWithoutMovingOthers() {
    auto group =
        Hybrid::Test::splitContainer("existing", "old", "source", "other-app");
    Hybrid::TopologyRepository repository(
        Hybrid::Test::topology({"new", "tile"}, {group}));
    Hybrid::Test::AlwaysReadyFactory scene;
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    const auto tab = planPlacement(repository.topology(), "source", "new",
                                   Placement::Tab, "tab");
    QVERIFY(tab.command);
    QVERIFY(coordinator.execute(*tab.command).committed());
    const auto *container = repository.topology().container("existing");
    QVERIFY(container);
    QCOMPARE(container->pages().at(0).root().toJson(),
             group.pages().at(0).root().toJson());
    const auto tile = planPlacement(repository.topology(), "new", "tile",
                                    Placement::TileDown, "tile");
    QVERIFY(tile.command);
    QVERIFY(coordinator.execute(*tile.command).committed());
    container = repository.topology().container("existing");
    QCOMPARE(container->pages().size(), 2);
    QCOMPARE(container->pages().at(0).root().toJson(),
             group.pages().at(0).root().toJson());
    QCOMPARE(container->pages().at(1).root().orientation(),
             Core::SplitOrientation::Vertical);
    QVERIFY(repository.topology().validate().valid);
  }
  void allTileDirectionsCreateSoloContainer_data() {
    QTest::addColumn<quint32>("mode");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<bool>("first");
    QTest::newRow("right") << quint32(Placement::TileRight) << false << false;
    QTest::newRow("down") << quint32(Placement::TileDown) << true << false;
    QTest::newRow("left") << quint32(Placement::TileLeft) << false << true;
    QTest::newRow("up") << quint32(Placement::TileUp) << true << true;
  }
  void allTileDirectionsCreateSoloContainer() {
    QFETCH(quint32, mode);
    QFETCH(bool, vertical);
    QFETCH(bool, first);
    Hybrid::TopologyRepository repository(
        Hybrid::Test::topology({"source", "new"}));
    Hybrid::Test::AlwaysReadyFactory scene;
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    const auto plan = planPlacement(repository.topology(), "source", "new",
                                    static_cast<Placement>(mode), "direction");
    QVERIFY(plan.command);
    QVERIFY(coordinator.execute(*plan.command).committed());
    const auto &root = repository.topology()
                           .container(plan.containerId)
                           ->pages()
                           .first()
                           .root();
    QCOMPARE(root.orientation(), vertical ? Core::SplitOrientation::Vertical
                                          : Core::SplitOrientation::Horizontal);
    QCOMPARE((first ? root.firstChild() : root.secondChild())->windowId(),
             QString("new"));
  }
  void rejectsStaleAndAlreadyOwnedTargets() {
    auto topology = Hybrid::Test::topology({"source", "new"});
    QVERIFY(!planPlacement(topology, "missing", "new", Placement::Tab, "n")
                 .command);
    QVERIFY(!planPlacement(topology, "source", "source", Placement::Tab, "n")
                 .command);
    QVERIFY(!planPlacement(topology, "source", "new",
                           static_cast<Placement>(99), "n")
                 .command);
    auto group =
        Hybrid::Test::splitContainer("existing", "old", "grouped", "other");
    auto owned = Hybrid::Test::topology({"source"}, {group});
    QVERIFY(!planPlacement(owned, "source", "grouped", Placement::Tab, "n")
                 .command);
  }
  void failedScenePublishesNoTopologyOrActivation() {
    class Transaction final : public Hybrid::SceneTransaction {
    public:
      Hybrid::SceneStepResult
      prepare(const Hybrid::WindowTopology &, const Hybrid::WindowTopology &,
              const Hybrid::TopologyCommand &) override {
        return Hybrid::SceneStepResult::ready();
      }
      Hybrid::SceneStepResult commit() override {
        return Hybrid::SceneStepResult::failure("fixture failure");
      }
      void rollback() noexcept override {}
    };
    class Factory final : public Hybrid::SceneTransactionFactory {
    public:
      std::unique_ptr<Hybrid::SceneTransaction> create() override {
        return std::make_unique<Transaction>();
      }
    } scene;
    Hybrid::TopologyRepository repository(
        Hybrid::Test::topology({"source", "new"}));
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    const auto plan = planPlacement(repository.topology(), "source", "new",
                                    Placement::Tab, "rollback");
    QVERIFY(plan.command);
    QVERIFY(!coordinator.execute(*plan.command).committed());
    QCOMPARE(repository.topology().revision(), quint64(0));
    QVERIFY(repository.topology().containerIds().isEmpty());
    QVERIFY(repository.topology().isIndependent("source"));
    QVERIFY(repository.topology().isIndependent("new"));
  }
};
QTEST_GUILESS_MAIN(PlacementTests)
#include "tst_placement.moc"
