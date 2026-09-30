// SPDX-License-Identifier: GPL-3.0-or-later
#include "grouping_fixtures.h"
#include <QTest>
#include <limits>
using namespace GroupingTest;
class PlannerTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void ownershipMatrix_data() {
        QTest::addColumn<int>("kind");QTest::addColumn<bool>("tab");
        for(int kind=0;kind<5;++kind) for(bool tab:{false,true})
            QTest::newRow(qPrintable(QString::number(kind)+(tab?"-tab":"-tile")))<<kind<<tab;
    }
    void ownershipMatrix() {
        QFETCH(int,kind);QFETCH(bool,tab);
        const auto before=scenario(kind);const auto original=fingerprint(before);
        const auto plan=planGrouping(before,request(kind,tab?GroupingMode::Tab:GroupingMode::Tile));
        QVERIFY2(plan.valid(),qPrintable(plan.error));QCOMPARE(fingerprint(before),original);
        const TopologyCommandKind expectedTab[]={TopologyCommandKind::GroupIndependentWindowsAsPages,TopologyCommandKind::InsertIndependentWindow,TopologyCommandKind::RegroupMemberWithIndependent,TopologyCommandKind::MoveMember,TopologyCommandKind::MoveMemberToPage};
        const TopologyCommandKind expectedTile[]={TopologyCommandKind::DockIndependentWindows,TopologyCommandKind::InsertIndependentWindow,TopologyCommandKind::RegroupMemberWithIndependent,TopologyCommandKind::MoveMember,TopologyCommandKind::ReparentMember};
        QCOMPARE(commandKind(*plan.command),tab?expectedTab[kind]:expectedTile[kind]);
        TopologyRepository repository(before);Hybrid::Test::AlwaysReadyFactory scene;
        TopologyCoordinator coordinator(repository,scene);const auto result=coordinator.execute(*plan.command);
        QVERIFY2(result.committed(),qPrintable(result.message));QCOMPARE(repository.topology().revision(),quint64{8});
        const auto *container=repository.topology().container(plan.containerId);QVERIFY(container);
        QVERIFY(container->findWindow("moved"));QVERIFY(container->findWindow(kind==4?"source-peer":"target"));
        if(kind>=2) QCOMPARE(container->findWindow("moved")->id(),QString("source-leaf-a"));
        if(tab) {
            const auto *page=memberPage(*container,"moved");QVERIFY(page);
            QCOMPARE(container->activePageId(),page->id());QVERIFY(page->root().isLeaf());
        } else {
            const auto *split=container->findNode("wm-group-"+Nonce+"-split");QVERIFY(split);
            QCOMPARE(split->ratio(),std::optional<double>(0.37));
            QCOMPARE(split->orientation(),std::optional(Core::SplitOrientation::Vertical));
            QCOMPARE(split->firstChild()->windowId(),QString("moved"));
        }
        QVERIFY(repository.topology().validate().valid);QVERIFY(roundTrip(repository.topology()));
    }
    void directionsUseFirstChildRatio() {
        for(int kind=0;kind<5;++kind) for(const auto direction:{GroupingDirection::Left,GroupingDirection::Right,GroupingDirection::Above,GroupingDirection::Below}) {
            TopologyRepository repository(scenario(kind));auto intent=request(kind,GroupingMode::Tile);intent.direction=direction;
            const auto plan=planGrouping(repository.topology(),intent);QVERIFY(plan.valid());
            Hybrid::Test::AlwaysReadyFactory scene;TopologyCoordinator coordinator(repository,scene);
            QVERIFY(coordinator.execute(*plan.command).committed());
            const auto *split=repository.topology().container(plan.containerId)->findNode("wm-group-"+Nonce+"-split");QVERIFY(split);
            QCOMPARE(split->ratio(),std::optional<double>(0.37));
            const bool horizontal=direction==GroupingDirection::Left || direction==GroupingDirection::Right;
            const bool first=direction==GroupingDirection::Left || direction==GroupingDirection::Above;
            QCOMPARE(split->orientation(),std::optional(horizontal?Core::SplitOrientation::Horizontal:Core::SplitOrientation::Vertical));
            QCOMPARE((first?split->firstChild():split->secondChild())->windowId(),QString("moved"));
            QVERIFY(roundTrip(repository.topology()));
        }
    }
    void rejectsMalformedInputsAndStructuralCollisions() {
        const auto value=scenario(0);const auto initial=fingerprint(value);
        auto good=request(0,GroupingMode::Tab);
        for(const auto &nonce:QStringList{"",Nonce.toUpper(),"{"+Nonce+"}","00000000-0000-0000-0000-000000000000","not-a-uuid"}) {
            auto bad=good;bad.nonce=nonce;QVERIFY(!planGrouping(value,bad).valid());
        }
        for(double ratio:{0.0,1.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            auto bad=good;bad.ratio=ratio;QVERIFY(!planGrouping(value,bad).valid());
        }
        auto bad=good;bad.direction=static_cast<GroupingDirection>(99);QVERIFY(!planGrouping(value,bad).valid());
        bad=good;bad.mode=static_cast<GroupingMode>(99);QVERIFY(!planGrouping(value,bad).valid());
        for(const auto &window:QStringList{"","unknown",QString(129,'x'),QString(QChar(0))}) {
            bad=good;bad.movedWindowId=window;QVERIFY(!planGrouping(value,bad).valid());
        }
        bad=good;bad.targetWindowId=bad.movedWindowId;QVERIFY(!planGrouping(value,bad).valid());
        QCOMPARE(fingerprint(value),initial);
        const auto collision=Hybrid::Test::splitContainer("wm-group-"+Nonce+"-container","collision","target","peer");
        const auto reused=Hybrid::Test::topology({"moved"},{collision});
        QVERIFY(!planGrouping(reused,good).valid());
        const auto source=Hybrid::Test::splitContainer("source","same","moved","source-peer");
        const auto target=Hybrid::Test::splitContainer("target-group","same","target","target-peer");
        const auto retainedCollision=Hybrid::Test::topology({},{source,target});
        QVERIFY(!planGrouping(retainedCollision,request(3,GroupingMode::Tile)).valid());
    }
    void samePlacementRejectsWithoutPublication() {
        TopologyRepository repository(scenario(4));const auto before=fingerprint(repository.topology());
        Hybrid::Test::AlwaysReadyFactory scene;TopologyCoordinator coordinator(repository,scene);
        auto intent=request(4,GroupingMode::Tile);intent.direction=GroupingDirection::Left;intent.ratio=0.5;
        const auto plan=planGrouping(repository.topology(),intent);QVERIFY(plan.valid());
        const auto result=coordinator.execute(*plan.command);QCOMPARE(result.error,TopologyCommandError::InvalidCommand);
        QCOMPARE(fingerprint(repository.topology()),before);
    }
};
QTEST_APPLESS_MAIN(PlannerTest)
#include "tst_planner.moc"
