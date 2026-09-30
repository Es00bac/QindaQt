// SPDX-License-Identifier: GPL-3.0-or-later
#include "grouping_fixtures.h"
#include <QTest>
#include <type_traits>
using namespace GroupingTest;
class Scene final:public SceneTransactionFactory {
public:
    bool failPrepare=false,failCommit=false,candidateActive=false;
    int created=0,prepared=0,committed=0,rolledBack=0;
    QString containerId;
    class Transaction final:public SceneTransaction {
    public:
        explicit Transaction(Scene &value):scene(value){}
        SceneStepResult prepare(const WindowTopology &,const WindowTopology &candidate,const TopologyCommand &) override {
            ++scene.prepared;const auto *container=candidate.container(scene.containerId);
            const auto *page=container?memberPage(*container,"moved"):nullptr;
            scene.candidateActive=page && container->activePageId()==page->id();
            return scene.failPrepare?SceneStepResult::failure("prepare sentinel"):SceneStepResult::ready();
        }
        SceneStepResult commit() override {++scene.committed;return scene.failCommit?SceneStepResult::failure("commit sentinel"):SceneStepResult::ready();}
        void rollback() noexcept override {++scene.rolledBack;}
    private:
        Scene &scene;
    };
    std::unique_ptr<SceneTransaction> create() override {++created;return std::make_unique<Transaction>(*this);}
};
class ActivationTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void tabActivationIsInsideEveryCandidate() {
        for(int kind=0;kind<5;++kind) {
            TopologyRepository repository(scenario(kind));const auto plan=planGrouping(repository.topology(),request(kind,GroupingMode::Tab));
            QVERIFY(plan.valid());Scene scene;scene.containerId=plan.containerId;TopologyCoordinator coordinator(repository,scene);
            const auto result=coordinator.execute(*plan.command);QVERIFY2(result.committed(),qPrintable(result.message));
            QCOMPARE(scene.created,1);QCOMPARE(scene.prepared,1);QCOMPARE(scene.committed,1);QVERIFY(scene.candidateActive);
            QVERIFY(roundTrip(repository.topology()));
        }
    }
    void rejectedSceneRestoresActivationOwnershipAndRevision_data() {
        QTest::addColumn<int>("kind");QTest::addColumn<bool>("prepareFailure");
        for(int kind=0;kind<5;++kind) for(bool prepare:{false,true})
            QTest::newRow(qPrintable(QString::number(kind)+(prepare?"-prepare":"-commit")))<<kind<<prepare;
    }
    void rejectedSceneRestoresActivationOwnershipAndRevision() {
        QFETCH(int,kind);QFETCH(bool,prepareFailure);
        TopologyRepository repository(scenario(kind));const auto before=fingerprint(repository.topology());
        const auto plan=planGrouping(repository.topology(),request(kind,GroupingMode::Tab));QVERIFY(plan.valid());
        Scene scene;scene.containerId=plan.containerId;scene.failPrepare=prepareFailure;scene.failCommit=!prepareFailure;
        TopologyCoordinator coordinator(repository,scene);const auto result=coordinator.execute(*plan.command);
        QCOMPARE(result.error,prepareFailure?TopologyCommandError::ScenePrepareFailed:TopologyCommandError::SceneCommitFailed);
        QCOMPARE(scene.created,1);QVERIFY(scene.candidateActive);QCOMPARE(scene.rolledBack,1);
        QCOMPARE(fingerprint(repository.topology()),before);QVERIFY(roundTrip(repository.topology()));
    }
    void invalidActivationDoesNotCreateScene() {
        TopologyRepository repository(scenario(3));const auto before=fingerprint(repository.topology());Scene scene;
        TopologyCoordinator coordinator(repository,scene);
        const auto invalid=coordinator.execute(MoveMember{"source","target-group","moved",MoveAsSplit{"target","fresh-split"},true});
        QCOMPARE(invalid.error,TopologyCommandError::InvalidCommand);QCOMPARE(scene.created,0);QCOMPARE(fingerprint(repository.topology()),before);
        TopologyRepository regroup(scenario(2));Scene regroupScene;TopologyCoordinator regroupCoordinator(regroup,regroupScene);
        const auto regroupBefore=fingerprint(regroup.topology());
        const auto invalidRegroup=regroupCoordinator.execute(RegroupMemberWithIndependent{"source","moved","target","new-group",RegroupAsSplit{"new-page","new-leaf","new-split"},true});
        QCOMPARE(invalidRegroup.error,TopologyCommandError::InvalidCommand);QCOMPARE(regroupScene.created,0);QCOMPARE(fingerprint(regroup.topology()),regroupBefore);
        const auto duplicatePage=regroupCoordinator.execute(MoveMemberToPage{"source","moved","source","source-page",true});
        QCOMPARE(duplicatePage.error,TopologyCommandError::InvalidCommand);QCOMPARE(regroupScene.created,0);QCOMPARE(fingerprint(regroup.topology()),regroupBefore);
        auto leafSource=Hybrid::Test::splitContainer("source","source","moved","source-peer");
        QVERIFY(leafSource.addPage("single-page","single-leaf","single-window"));
        TopologyRepository sole(Hybrid::Test::topology({},{leafSource}));Scene soleScene;TopologyCoordinator soleCoordinator(sole,soleScene);
        const auto soleBefore=fingerprint(sole.topology());
        const auto noOp=soleCoordinator.execute(MoveMemberToPage{"source","single-window","new-page","single-page",true});
        QCOMPARE(noOp.error,TopologyCommandError::InvalidCommand);QCOMPARE(soleScene.created,0);QCOMPARE(fingerprint(sole.topology()),soleBefore);
    }
    void omittedFlagsPreserveExistingTabPolicy() {
        for(int kind:{2,3,4}) {
            TopologyRepository repository(scenario(kind));auto plan=planGrouping(repository.topology(),request(kind,GroupingMode::Tab));QVERIFY(plan.valid());
            std::visit([](auto &command) {
                using T=std::decay_t<decltype(command)>;
                if constexpr(std::is_same_v<T,MoveMember> || std::is_same_v<T,MoveMemberToPage>) command.activateMovedPage=false;
                if constexpr(std::is_same_v<T,RegroupMemberWithIndependent>) command.activateMemberPage=false;
            },*plan.command);
            Scene scene;scene.containerId=plan.containerId;TopologyCoordinator coordinator(repository,scene);
            QVERIFY(coordinator.execute(*plan.command).committed());QVERIFY(!scene.candidateActive);QVERIFY(roundTrip(repository.topology()));
        }
    }
};
QTEST_APPLESS_MAIN(ActivationTest)
#include "tst_activation.moc"
