// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/console_send_fixture.h"
#include "wireplumber_routing_p.h"
#include "wireplumber_send_image_p.h"
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;
namespace {
BackendRoutingEdge edge(const QString &strip = QStringLiteral("strip.virtual.1"),
                        const QString &bus = QStringLiteral("bus.a1"), quint64 target = 10)
{
    return {.stripId = strip, .busId = bus, .source = {7, 20}, .target = {7, target},
        .sourceIsSink = true, .gainDb = -6.0, .pan = 0.0, .audible = true,
        .stripGainDb = 0.0, .busGainDb = 0.0};
}
}
class ConsoleSendReconcilerTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultBuildCannotEnableBorrowedCore();
    void staysQuietUntilBothActualRoles();
    void independentApplicationsAndOutputs();
    void fadersPanMuteReuseTheAdmittedModule();
    void retargetWaitsForActualDestruction();
    void copiedForeignIdentityAndOwnedDuplicate();
    void replacementAndProducerLossRevoke();
    void refusedSetterAndMalformedDeclarationRetire();
    void stopIsIdempotentAcrossPendingRetirement();
};

void ConsoleSendReconcilerTests::defaultBuildCannotEnableBorrowedCore()
{
    if (ConsoleSendImage::enabled()) QSKIP("This gate checks the default-OFF source build");
    QVERIFY(!ConsoleSendImage{}.admitted());
    QVERIFY(!ConsoleSendImage{}.loaded(nullptr));
}

void ConsoleSendReconcilerTests::staysQuietUntilBothActualRoles()
{
    ConsoleSendFixture fixture;
    fixture.announce = false;
    ConsoleSendReconciler controller;
    controller.reconcile({edge()}, fixture.runtime());
    QCOMPARE(fixture.modules.size(), size_t(1));
    QVERIFY(fixture.writes.isEmpty());
    const auto &arguments = fixture.modules.front()->arguments;
    QVERIFY(arguments.contains("mute = true channelVolumes = [ 0.0 0.0 ]"));
    QVERIFY(arguments.contains("state.restore-props = false"));
    fixture.publishPair(*fixture.modules.front());
    const auto playback = fixture.nodes.takeLast();
    controller.reconcile({edge()}, fixture.runtime());
    QVERIFY(fixture.writes.isEmpty());
    fixture.nodes.append(playback);
    controller.reconcile({edge()}, fixture.runtime());
    QCOMPARE(fixture.writes.size(), 1);
    QVERIFY(std::abs(fixture.writes.last().amplitude.left - std::pow(10.0, -6.0 / 20.0)) < 1e-10);
    QCOMPARE(fixture.writes.last().node, playback);
}

void ConsoleSendReconcilerTests::independentApplicationsAndOutputs()
{
    ConsoleSendFixture fixture;
    ConsoleSendReconciler controller;
    QList<BackendRoutingEdge> routes;
    for (int app = 1; app <= 3; ++app) {
        auto route = edge(QStringLiteral("strip.virtual.%1").arg(app),
                          QStringLiteral("bus.a%1").arg(app), static_cast<quint64>(app + 9));
        route.source.serial = static_cast<quint64>(app + 19);
        route.stripGainDb = -3.0 * app;
        routes.append(route);
    }
    auto other = routes.first();
    other.busId = QStringLiteral("bus.b1");
    other.target.serial = 50;
    other.busGainDb = 12.0;
    routes.append(other);
    controller.reconcile(routes, fixture.runtime());
    QCOMPARE(controller.size(), qsizetype(4));
    QCOMPARE(fixture.writes.size(), 4);
    for (const auto &module : fixture.modules) {
        const auto route = std::find_if(routes.cbegin(), routes.cend(), [&](const auto &value) {
            return routingNodeName(value.stripId, value.busId) == module->key;
        });
        QVERIFY(route != routes.cend());
        QVERIFY(module->arguments.contains(QStringLiteral("target.object = \"output.%1\"")
                                           .arg(route->target.serial).toUtf8()));
        QVERIFY(module->arguments.contains(QStringLiteral("target.object = \"source.%1\"")
                                           .arg(route->source.serial).toUtf8()));
        const auto write = std::find_if(fixture.writes.cbegin(), fixture.writes.cend(), [&](const auto &value) {
            return value.node.name == module->key;
        });
        QVERIFY(write != fixture.writes.cend());
        const double expected = std::pow(10.0, (route->stripGainDb + route->gainDb + route->busGainDb) / 20.0);
        QVERIFY(std::abs(write->amplitude.left - expected) < 1e-10);
    }
}

void ConsoleSendReconcilerTests::fadersPanMuteReuseTheAdmittedModule()
{
    ConsoleSendFixture fixture;
    ConsoleSendReconciler controller;
    auto route = edge();
    controller.reconcile({route}, fixture.runtime());
    route.stripGainDb = 12.0;
    route.busGainDb = -6.0;
    route.pan = -1.0;
    controller.reconcile({route}, fixture.runtime());
    QCOMPARE(fixture.modules.size(), size_t(1));
    QVERIFY(std::abs(fixture.writes.last().amplitude.left - 1.0) < 1e-10);
    QCOMPARE(fixture.writes.last().amplitude.right, 0.0);
    route.audible = false;
    controller.reconcile({route}, fixture.runtime());
    QCOMPARE(fixture.writes.last().amplitude.left, 0.0);
    route.audible = true;
    route.gainDb = -60.0;
    controller.reconcile({route}, fixture.runtime());
    QCOMPARE(fixture.writes.last().amplitude.left, 0.0);
    QCOMPARE(fixture.modules.size(), size_t(1));
}

void ConsoleSendReconcilerTests::retargetWaitsForActualDestruction()
{
    ConsoleSendFixture fixture;
    ConsoleSendReconciler controller;
    auto route = edge();
    controller.reconcile({route}, fixture.runtime());
    const QByteArray oldArguments = fixture.modules.front()->arguments;
    route.target.serial = 11;
    controller.reconcile({route}, fixture.runtime());
    QCOMPARE(fixture.retired.size(), 1);
    QCOMPARE(fixture.writes.last().amplitude.left, 0.0);
    QCOMPARE(fixture.modules.size(), size_t(1));
    controller.reconcile({route}, fixture.runtime());
    QCOMPARE(fixture.modules.size(), size_t(1));
    fixture.completeRetirement(controller, fixture.retired.first());
    controller.reconcile({route}, fixture.runtime());
    QCOMPARE(fixture.modules.size(), size_t(2));
    QVERIFY(fixture.modules.back()->arguments != oldArguments);
    QVERIFY(fixture.modules.back()->arguments.contains("target.object = \"output.11\""));
}

void ConsoleSendReconcilerTests::copiedForeignIdentityAndOwnedDuplicate()
{
    ConsoleSendFixture fixture;
    ConsoleSendReconciler controller;
    controller.reconcile({edge()}, fixture.runtime());
    const auto original = fixture.nodes.last();
    auto foreign = original;
    foreign.clientId = 99; foreign.serial = 900; foreign.objectIdentity = 900;
    fixture.nodes.append(foreign);
    controller.reconcile({edge()}, fixture.runtime());
    QCOMPARE(fixture.writes.last().node, original);
    QVERIFY(!controller.admission().excludes(foreign.serial, foreign.objectIdentity));
    auto duplicate = original;
    duplicate.serial = 901; duplicate.objectIdentity = 901;
    fixture.nodes.append(duplicate);
    controller.reconcile({edge()}, fixture.runtime());
    QVERIFY(controller.refused());
    QCOMPARE(fixture.retired.size(), 1);
    QCOMPARE(fixture.writes.last().node, original);
    QCOMPARE(fixture.writes.last().amplitude.left, 0.0);
}

void ConsoleSendReconcilerTests::replacementAndProducerLossRevoke()
{
    ConsoleSendFixture fixture;
    ConsoleSendReconciler controller;
    controller.reconcile({edge()}, fixture.runtime());
    fixture.nodes.last().serial += 100;
    fixture.writes.clear();
    controller.reconcile({edge()}, fixture.runtime());
    QVERIFY(controller.refused());
    QVERIFY(fixture.writes.isEmpty());
    fixture.completeRetirement(controller, fixture.retired.last());
    controller.reconcile({edge()}, fixture.runtime());
    const qsizetype before = fixture.writes.size();
    fixture.producer = {};
    controller.reconcile({edge()}, fixture.runtime());
    QCOMPARE(fixture.writes.size(), before);
    QVERIFY(controller.refused());
    QCOMPARE(controller.size(), qsizetype(0));
}

void ConsoleSendReconcilerTests::refusedSetterAndMalformedDeclarationRetire()
{
    ConsoleSendFixture fixture;
    ConsoleSendReconciler controller;
    fixture.acceptWrite = false;
    controller.reconcile({edge()}, fixture.runtime());
    QVERIFY(controller.refused());
    QCOMPARE(fixture.retired.size(), 1);
    QCOMPARE(controller.size(), qsizetype(0));
    fixture.completeRetirement(controller, fixture.retired.last());
    fixture.acceptWrite = true;
    controller.reconcile({edge()}, fixture.runtime());
    auto invalid = edge();
    invalid.stripGainDb = std::numeric_limits<double>::quiet_NaN();
    controller.reconcile({invalid}, fixture.runtime());
    QVERIFY(controller.refused());
    QCOMPARE(fixture.writes.last().amplitude.left, 0.0);
    QCOMPARE(controller.size(), qsizetype(0));
}

void ConsoleSendReconcilerTests::stopIsIdempotentAcrossPendingRetirement()
{
    ConsoleSendFixture fixture;
    ConsoleSendReconciler controller;
    controller.reconcile({edge()}, fixture.runtime());
    controller.reconcile({}, fixture.runtime());
    controller.stop(fixture.runtime());
    QCOMPARE(fixture.destroyed.size(), 1);
    controller.stop(fixture.runtime());
    QCOMPARE(fixture.destroyed.size(), 1);
    QVERIFY(!controller.admission().excludes(fixture.nodes.last().serial,
                                           fixture.nodes.last().objectIdentity));
}
QTEST_GUILESS_MAIN(ConsoleSendReconcilerTests)
#include "tst_console_send_reconciler.moc"
