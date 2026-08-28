// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwindevelopmentoutputseam.h"

#include <core/backendoutput.h>
#include <core/outputbackend.h>

#include <QJsonDocument>
#include <QSet>
#include <QtTest>

#include <limits>
#include <utility>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

QString status(const QByteArray &payload)
{
    return QJsonDocument::fromJson(payload).object().value(QStringLiteral("status")).toString();
}

QString code(const QByteArray &payload)
{
    return QJsonDocument::fromJson(payload).object()
        .value(QStringLiteral("failure")).toObject()
        .value(QStringLiteral("code")).toString();
}

class FakeMutator final : public DevelopmentOutputMutator
{
public:
    bool isAvailable() const override { ++queries; return available; }
    qsizetype totalOutputCount() const override { ++queries; return total; }
    qsizetype ownedOutputCount() const override { ++queries; return owned.size(); }
    bool outputNameExists(const QString &name) const override
    { ++queries; return external.contains(name); }
    bool ownsOutput(const QString &name) const override
    { ++queries; return owned.contains(name); }
    DevelopmentOutputMutationResult addVirtualOutput(
        const QString &name, const QSize &, qreal) override
    {
        ++mutations;
        if (addResult == DevelopmentOutputMutationResult::Succeeded) {
            owned.insert(name);
            ++total;
        }
        return addResult;
    }
    DevelopmentOutputMutationResult removeVirtualOutput(const QString &name) override
    {
        ++mutations;
        if (removeResult == DevelopmentOutputMutationResult::Succeeded) {
            owned.remove(name);
            --total;
        }
        return removeResult;
    }
    void removeOwnedOutputs() override
    {
        ++shutdowns;
        total -= owned.size();
        owned.clear();
    }

    mutable int queries = 0;
    int mutations = 0;
    int shutdowns = 0;
    qsizetype total = 1;
    bool available = true;
    QSet<QString> external;
    QSet<QString> owned;
    DevelopmentOutputMutationResult addResult = DevelopmentOutputMutationResult::Succeeded;
    DevelopmentOutputMutationResult removeResult = DevelopmentOutputMutationResult::Succeeded;
};

class FakeBackendOutput final : public KWin::BackendOutput
{
public:
    explicit FakeBackendOutput(QString name)
    {
        Information information;
        information.name = std::move(name);
        setInformation(information);
    }

    KWin::RenderLoop *renderLoop() const override { return nullptr; }
    bool testPresentation(const std::shared_ptr<KWin::OutputFrame> &) override
    {
        return false;
    }
    bool present(const QList<KWin::OutputLayer *> &,
                 const std::shared_ptr<KWin::OutputFrame> &) override
    {
        return false;
    }
};

class FakeOutputBackend final : public KWin::OutputBackend
{
public:
    ~FakeOutputBackend() override
    {
        while (!backendOutputs.isEmpty()) {
            backendOutputs.takeLast()->unref();
        }
    }

    bool initialize() override { return true; }
    KWin::EglDisplay *sceneEglDisplayObject() const override { return nullptr; }
    QList<KWin::CompositingType> supportedCompositors() const override
    {
        return {KWin::QPainterCompositing};
    }
    QList<KWin::BackendOutput *> outputs() const override { return backendOutputs; }
    KWin::BackendOutput *createVirtualOutput(
        const QString &name, const QString &, const QSize &, qreal) override
    {
        ++createCalls;
        if (rejectCreate) {
            return nullptr;
        }
        auto *const output = new FakeBackendOutput(
            QStringLiteral("Virtual-%1").arg(name));
        backendOutputs.append(output);
        return output;
    }
    void removeVirtualOutput(KWin::BackendOutput *output) override
    {
        ++removeCalls;
        if (!ignoreRemove && backendOutputs.removeOne(output)) {
            output->unref();
        }
    }
    KWin::BackendOutput *addExternal(QString name)
    {
        auto *const output = new FakeBackendOutput(std::move(name));
        backendOutputs.append(output);
        return output;
    }

    QList<KWin::BackendOutput *> backendOutputs;
    int createCalls = 0;
    int removeCalls = 0;
    bool rejectCreate = false;
    bool ignoreRemove = false;
};

} // namespace

class DevelopmentOutputProtocolTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void productionRejectsBeforeParseOrBackend();
    void validatesAndMutatesOwnedOutputs();
    void preservesOwnershipOnBackendFailure();
    void enforcesCountsAndTeardownIsIdempotent();
    void exactAdapterRejectsPrefixedCollision();
    void exactAdapterTracksPointerAndPreservesExternalOutputs();
    void exactAdapterRetainsAuthorityOnRemoveNoOp();
};

void DevelopmentOutputProtocolTest::productionRejectsBeforeParseOrBackend()
{
    FakeMutator backend;
    DevelopmentOutputController controller(false, &backend);
    const auto malformed = controller.addVirtualOutputForTest(
        QString(500, QLatin1Char('\0')), -1, -1,
        std::numeric_limits<double>::quiet_NaN());
    const auto valid = controller.addVirtualOutputForTest(
        QStringLiteral("test"), 1920, 1080, 1.5);
    QCOMPARE(code(malformed), QStringLiteral("control-disabled"));
    QCOMPARE(malformed, valid);
    QCOMPARE(code(controller.removeVirtualOutputForTest(QString(500, QLatin1Char('\0')))),
             QStringLiteral("control-disabled"));
    QCOMPARE(backend.queries, 0);
    QCOMPARE(backend.mutations, 0);
}

void DevelopmentOutputProtocolTest::validatesAndMutatesOwnedOutputs()
{
    FakeMutator backend;
    DevelopmentOutputController controller(true, &backend);
    QCOMPARE(code(controller.addVirtualOutputForTest(QStringLiteral("bad name"), 800, 600, 1)),
             QStringLiteral("malformed-virtual-output-request"));
    QCOMPARE(backend.mutations, 0);
    QCOMPARE(status(controller.addVirtualOutputForTest(
                 QStringLiteral("test-1"), 1920, 1080, 1.5)),
             QStringLiteral("added"));
    QVERIFY(backend.owned.contains(QStringLiteral("test-1")));
    QCOMPARE(code(controller.addVirtualOutputForTest(
                 QStringLiteral("test-1"), 1920, 1080, 1.5)),
             QStringLiteral("output-name-in-use"));
    QCOMPARE(status(controller.removeVirtualOutputForTest(QStringLiteral("test-1"))),
             QStringLiteral("removed"));
    QCOMPARE(code(controller.removeVirtualOutputForTest(QStringLiteral("test-1"))),
             QStringLiteral("unknown-virtual-output"));
}

void DevelopmentOutputProtocolTest::preservesOwnershipOnBackendFailure()
{
    FakeMutator backend;
    DevelopmentOutputController controller(true, &backend);
    backend.addResult = DevelopmentOutputMutationResult::BackendRejected;
    QCOMPARE(code(controller.addVirtualOutputForTest(
                 QStringLiteral("test"), 800, 600, 1)),
             QStringLiteral("virtual-output-backend-rejected"));
    QVERIFY(backend.owned.isEmpty());
    backend.owned.insert(QStringLiteral("owned"));
    backend.removeResult = DevelopmentOutputMutationResult::BackendRejected;
    QCOMPARE(code(controller.removeVirtualOutputForTest(QStringLiteral("owned"))),
             QStringLiteral("virtual-output-backend-rejected"));
    QVERIFY(backend.owned.contains(QStringLiteral("owned")));
}

void DevelopmentOutputProtocolTest::enforcesCountsAndTeardownIsIdempotent()
{
    FakeMutator backend;
    DevelopmentOutputController controller(true, &backend);
    backend.total = DevelopmentOutputController::MaxTotalOutputs;
    QCOMPARE(code(controller.addVirtualOutputForTest(
                 QStringLiteral("test"), 800, 600, 1)),
             QStringLiteral("virtual-output-limit"));
    backend.total = 2;
    backend.owned = {QStringLiteral("one")};
    controller.shutdown();
    controller.shutdown();
    QCOMPARE(backend.shutdowns, 1);
    QVERIFY(backend.owned.isEmpty());
    QCOMPARE(code(controller.addVirtualOutputForTest(
                 QStringLiteral("test"), 800, 600, 1)),
             QStringLiteral("control-disabled"));
}

void DevelopmentOutputProtocolTest::exactAdapterRejectsPrefixedCollision()
{
    FakeOutputBackend backend;
    backend.addExternal(QStringLiteral("Virtual-collision"));
    KWinDevelopmentOutputSeam seam(&backend);
    DevelopmentOutputController controller(true, &seam);
    QCOMPARE(code(controller.addVirtualOutputForTest(
                 QStringLiteral("collision"), 800, 600, 1)),
             QStringLiteral("output-name-in-use"));
    QCOMPARE(backend.createCalls, 0);
    QCOMPARE(backend.outputs().size(), 1);
}

void DevelopmentOutputProtocolTest::exactAdapterTracksPointerAndPreservesExternalOutputs()
{
    FakeOutputBackend backend;
    auto *const external = backend.addExternal(QStringLiteral("Virtual-external"));
    KWinDevelopmentOutputSeam seam(&backend);
    DevelopmentOutputController controller(true, &seam);
    QCOMPARE(status(controller.addVirtualOutputForTest(
                 QStringLiteral("owned"), 800, 600, 1.25)),
             QStringLiteral("added"));
    QVERIFY(seam.ownsOutput(QStringLiteral("owned")));
    QCOMPARE(status(controller.removeVirtualOutputForTest(QStringLiteral("owned"))),
             QStringLiteral("removed"));
    QVERIFY(!seam.ownsOutput(QStringLiteral("owned")));
    QCOMPARE(backend.outputs(), QList<KWin::BackendOutput *>{external});

    QCOMPARE(status(controller.addVirtualOutputForTest(
                 QStringLiteral("teardown"), 800, 600, 1)),
             QStringLiteral("added"));
    controller.shutdown();
    QCOMPARE(backend.outputs(), QList<KWin::BackendOutput *>{external});
}

void DevelopmentOutputProtocolTest::exactAdapterRetainsAuthorityOnRemoveNoOp()
{
    FakeOutputBackend backend;
    KWinDevelopmentOutputSeam seam(&backend);
    DevelopmentOutputController controller(true, &seam);
    QCOMPARE(status(controller.addVirtualOutputForTest(
                 QStringLiteral("owned"), 800, 600, 1)),
             QStringLiteral("added"));
    backend.ignoreRemove = true;
    QCOMPARE(code(controller.removeVirtualOutputForTest(QStringLiteral("owned"))),
             QStringLiteral("virtual-output-backend-rejected"));
    QVERIFY(seam.ownsOutput(QStringLiteral("owned")));
    backend.ignoreRemove = false;
    QCOMPARE(status(controller.removeVirtualOutputForTest(QStringLiteral("owned"))),
             QStringLiteral("removed"));
}

QTEST_GUILESS_MAIN(DevelopmentOutputProtocolTest)
#include "tst_kwindevelopmentoutputseam.moc"
