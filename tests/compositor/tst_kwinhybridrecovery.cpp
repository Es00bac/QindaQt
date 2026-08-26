// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene_testfixture.h"

#include "qindaqt/hybrid/topologycoordinator.h"

#include <QTest>

using namespace QindaQt;
using namespace QindaQt::Compositor;

namespace {

Hybrid::WindowTopology independentTopology()
{
    QString error;
    auto topology = Hybrid::WindowTopology::create(
        {QStringLiteral("a"), QStringLiteral("b")}, {}, 0, &error);
    Q_ASSERT_X(topology.has_value(), "recovery fixture", qPrintable(error));
    return std::move(*topology);
}

Hybrid::DockIndependentWindows dockCommand()
{
    return {
        .containerId = QStringLiteral("group"),
        .pageId = QStringLiteral("page"),
        .firstWindowId = QStringLiteral("a"),
        .firstLeafNodeId = QStringLiteral("leaf-a"),
        .secondWindowId = QStringLiteral("b"),
        .secondLeafNodeId = QStringLiteral("leaf-b"),
        .splitNodeId = QStringLiteral("split"),
        .orientation = Core::SplitOrientation::Horizontal,
        .ratio = 0.5,
        .secondPosition = Core::InsertPosition::Second,
    };
}

} // namespace

class KWinHybridRecoveryTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emergencyReleaseRestoresStateAndOwnership();
    void emergencyReleaseToleratesConcurrentClose();
};

void KWinHybridRecoveryTest::emergencyReleaseRestoresStateAndOwnership()
{
    Test::FakeHybridScenePlatform platform;
    auto first = Test::richState(QRectF(20, 30, 600, 400),
                                 QStringLiteral("output-a"), true);
    first.skipTaskbar = true;
    auto second = Test::richState(QRectF(700, 40, 500, 390),
                                  QStringLiteral("output-b"));
    second.skipSwitcher = true;
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(independentTopology());
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dockCommand()).committed());

    const auto result = factory.emergencyReleaseAll(repository.topology());

    QVERIFY2(result.succeeded, qPrintable(result.message));
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, first);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state, second);
    QVERIFY(platform.owners.value(QStringLiteral("a")).isEmpty());
    QVERIFY(platform.owners.value(QStringLiteral("b")).isEmpty());
    QVERIFY(!factory.committedLayout(QStringLiteral("group")));
}

void KWinHybridRecoveryTest::emergencyReleaseToleratesConcurrentClose()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 600, 400),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(700, 40, 500, 390),
                                        QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(independentTopology());
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dockCommand()).committed());
    platform.removeWindow(QStringLiteral("b"));

    const auto result = factory.emergencyReleaseAll(repository.topology());

    QVERIFY2(result.succeeded, qPrintable(result.message));
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, first);
    QVERIFY(platform.owners.value(QStringLiteral("a")).isEmpty());
}

QTEST_GUILESS_MAIN(KWinHybridRecoveryTest)
#include "tst_kwinhybridrecovery.moc"
