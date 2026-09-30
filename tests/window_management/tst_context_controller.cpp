// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/window_management/controller.h>
#include <QTest>
using namespace QindaQt::WindowManagement;
namespace {
const QByteArray Focus=R"({"version":1,"operation":"focus","target":{"kind":"current"},"arguments":{}})";
struct Permit final : Authority {
    bool enabled=true, screenLocked=false;
    QString owner=QStringLiteral(":1.10");
    bool authorized(const QString &caller) const override { return enabled && caller==owner; }
    bool locked() const override { return screenLocked; }
};
struct World final : Scene {
    bool fresh=true, emptyDesktop=false;
    mutable int captures=0,resolutions=0;
    Resolution resolution{Status::Accepted,{"window-a","container-a"},{},{}};
    std::optional<ContextSnapshot> capture() const override { ++captures;return emptyDesktop ? ContextSnapshot{} : ContextSnapshot{"window-a","container-a",3}; }
    bool current(const ContextSnapshot &) const override { return fresh; }
    Resolution resolve(const Target &,const ContextSnapshot &) const override { ++resolutions;return resolution; }
    QStringList capabilities() const override { return {"focus","maximize"}; }
};
struct Apply final : Executor {
    int calls=0;
    Result execute(const Command &,const ResolvedTarget &) override { ++calls;return {Status::Accepted,{},{},{},{},{}}; }
};
struct Fixture {
    Permit permit; World world; Apply apply; qint64 now=5000;int nonce=0;
    Controller controller{permit,world,apply,[this]{return now;},[this]{return QString::number(++nonce);}};
    QString begin() { return controller.begin(permit.owner).contextId; }
};
}
class ContextControllerTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void authorizedOnceOnly()
    {
        Fixture f;const auto id=f.begin();QVERIFY(!id.isEmpty());
        QCOMPARE(f.controller.submit(f.permit.owner,id,Focus).status,Status::Accepted);
        QCOMPARE(f.controller.submit(f.permit.owner,id,Focus).status,Status::Stale);
        QCOMPARE(f.apply.calls,1);
    }
    void emptyDesktopStillCapturesExplicitIntent()
    {
        Fixture f; f.world.emptyDesktop=true;
        const auto result=f.controller.begin(f.permit.owner);
        QCOMPARE(result.status,Status::Accepted);
        QVERIFY(!result.contextId.isEmpty()); QVERIFY(result.windowId.isEmpty());
        f.world.fresh=false;
        QCOMPARE(f.controller.submit(f.permit.owner,result.contextId,Focus).status,Status::Stale);
        QCOMPARE(f.apply.calls,0);
    }
    void rejectsBeforeCaptureOrParsing()
    {
        Fixture f;f.permit.enabled=false;
        QCOMPARE(f.controller.begin(f.permit.owner).status,Status::Denied);
        QCOMPARE(f.controller.submit(f.permit.owner,"x",QByteArray(MaximumRequestBytes+1,'x')).status,Status::Denied);
        QCOMPARE(f.world.captures,0);QCOMPARE(f.world.resolutions,0);QCOMPARE(f.apply.calls,0);
    }
    void replacementAndRevocationCannotRevive()
    {
        Fixture f;const auto first=f.begin();const auto next=f.begin();
        QCOMPARE(f.controller.submit(f.permit.owner,first,Focus).status,Status::Stale);
        f.controller.invalidate();
        QCOMPARE(f.controller.submit(f.permit.owner,next,Focus).status,Status::Stale);
        const auto third=f.begin();f.permit.owner=":1.20";
        QCOMPARE(f.controller.submit(f.permit.owner,third,Focus).status,Status::Stale);
        QCOMPARE(f.apply.calls,0);
    }
    void expiredStaleAndLocked()
    {
        Fixture f;auto id=f.begin();f.now+=60'000;
        QCOMPARE(f.controller.submit(f.permit.owner,id,Focus).status,Status::Stale);
        id=f.begin();f.world.fresh=false;
        QCOMPARE(f.controller.submit(f.permit.owner,id,Focus).status,Status::Stale);
        f.world.fresh=true;id=f.begin();f.permit.screenLocked=true;
        QCOMPARE(f.controller.submit(f.permit.owner,id,Focus).status,Status::Denied);
        f.permit.screenLocked=false;
        QCOMPARE(f.controller.submit(f.permit.owner,id,Focus).status,Status::Stale);
        QCOMPARE(f.apply.calls,0);
    }
    void ambiguityDoesNotMutateOrReplay()
    {
        Fixture f;const auto id=f.begin();f.world.resolution={Status::Ambiguous,{},"choose a unique container",{"Research A","Research B"}};
        const auto result=f.controller.submit(f.permit.owner,id,Focus);
        QCOMPARE(result.status,Status::Ambiguous);QCOMPARE(result.candidates.size(),2);
        QCOMPARE(f.apply.calls,0);
        QCOMPARE(f.controller.submit(f.permit.owner,id,Focus).status,Status::Stale);
    }
    void invalidUnsupportedCancelAndFlood()
    {
        Fixture f;auto id=f.begin();
        QCOMPARE(f.controller.submit(f.permit.owner,id,"{}").status,Status::Invalid);
        QCOMPARE(f.apply.calls,0);
        id=f.begin();QCOMPARE(f.controller.cancel(f.permit.owner,id).status,Status::Cancelled);
        QCOMPARE(f.controller.submit(f.permit.owner,id,Focus).status,Status::Stale);
        f.now+=1001;id=f.begin();
        const QByteArray close=R"({"version":1,"operation":"close","target":{"kind":"current"},"arguments":{}})";
        QCOMPARE(f.controller.submit(f.permit.owner,id,close).status,Status::Unavailable);
        for(int i=0;i<6;++i) static_cast<void>(f.begin());
        QCOMPARE(f.controller.begin(f.permit.owner).status,Status::ResourceLimit);
        QCOMPARE(f.apply.calls,0);
    }
};
QTEST_GUILESS_MAIN(ContextControllerTest)
#include "tst_context_controller.moc"
