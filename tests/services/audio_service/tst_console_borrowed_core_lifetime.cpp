// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/gain_native_graph.h"
#include "support/gain_private_runtime.h"
#include "wireplumber_routing_p.h"
#include <pipewire/impl-module.h>
#include <QtCore/QUuid>
#include <QtTest>

extern "C" void qindaqt_gain_fail_next_stream_new();
extern "C" unsigned qindaqt_gain_stream_failures();
using namespace QindaQt::Audio;
using namespace QindaQt::Audio::Test;

namespace {
constexpr char kSource[] = "gain.lifetime.source";
constexpr char kTarget[] = "gain.lifetime.target";
struct ModuleProbe {
    spa_hook listener{};
    bool destroyed = false;
};
void moduleDestroyed(void *data)
{
    auto *probe = static_cast<ModuleProbe *>(data);
    spa_hook_remove(&probe->listener);
    probe->destroyed = true;
}
void watch(pw_impl_module *module, ModuleProbe &probe)
{
    static const pw_impl_module_events events = [] {
        pw_impl_module_events value{};
        value.version = PW_VERSION_IMPL_MODULE_EVENTS;
        value.destroy = moduleDestroyed;
        return value;
    }();
    pw_impl_module_add_listener(module, &probe.listener, &events, &probe);
}
QByteArray arguments()
{
    const SendModuleDeclaration declaration{.instance = 1,
        .token = QUuid::createUuid().toString(QUuid::Id128),
        .playbackName = routingNodeName(QStringLiteral("life"), QStringLiteral("bus"))};
    return ownedRoutingModuleArguments(declaration, QStringLiteral("life"), QStringLiteral("bus"),
        QString::fromLatin1(kSource), QString::fromLatin1(kTarget), true);
}
bool prepare(GainNativeGraph &graph)
{
    return graph.start() && graph.createSink(QString::fromLatin1(kSource))
        && graph.createSink(QString::fromLatin1(kTarget));
}
} // namespace

class ConsoleBorrowedCoreLifetimeTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void moduleFirstThenCoreStop();
    void ownedAndBorrowedProducerIdentity_data();
    void ownedAndBorrowedProducerIdentity();
    void coreFirstThenDeferredModuleTeardown();
    void failedInitializationAfterCoreHooks();
    void repeatedSharedCoreModuleChurn();
    void moduleOverrideAfterBootstrapRefusesBeforeFactory();
};

void ConsoleBorrowedCoreLifetimeTests::moduleFirstThenCoreStop()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    ModuleProbe probe; // outlives graph even when a QVERIFY returns early
    GainNativeGraph graph(runtime); QVERIFY2(prepare(graph), qPrintable(graph.failure()));
    auto *module = graph.load(arguments()); QVERIFY(module); watch(module, probe);
    graph.destroy(module);
    QVERIFY(probe.destroyed);
    // Original upstream keeps its impl hooks linked to this live shared Core.
    // The following actual disconnect is the immutable ASan red control.
    QVERIFY(wp_core_is_connected(graph.core()));
    graph.settle(100);
    graph.disconnect();
    graph.disconnect();
}

void ConsoleBorrowedCoreLifetimeTests::ownedAndBorrowedProducerIdentity_data()
{
    QTest::addColumn<bool>("borrowed");
    QTest::newRow("standalone-owned-core") << false;
    QTest::newRow("shared-borrowed-core") << true;
}
void ConsoleBorrowedCoreLifetimeTests::ownedAndBorrowedProducerIdentity()
{
    QFETCH(bool, borrowed);
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    ModuleProbe probe;
    GainNativeGraph graph(runtime); QVERIFY2(prepare(graph), qPrintable(graph.failure()));
    const auto producer = graph.producer(); QVERIFY(producer.valid());
    auto *module = borrowed ? graph.load(arguments()) : graph.loadStandalone(arguments());
    QVERIFY(module); watch(module, probe);
    QVERIFY(graph.wait([&] { return graph.nodes().size() >= 2; }));
    bool playbackFound = false;
    for (const auto &node : graph.nodes()) {
        if (node.role != QLatin1String("playback")) continue;
        playbackFound = true;
        QVERIFY(node.clientId != 0);
        QCOMPARE(node.clientId == producer.clientId, borrowed);
    }
    QVERIFY(playbackFound);
    graph.destroy(module); QVERIFY(probe.destroyed);
    QVERIFY(wp_core_is_connected(graph.core()));
    graph.disconnect();
}

void ConsoleBorrowedCoreLifetimeTests::coreFirstThenDeferredModuleTeardown()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    ModuleProbe probe;
    GainNativeGraph graph(runtime); QVERIFY2(prepare(graph), qPrintable(graph.failure()));
    auto *module = graph.load(arguments()); QVERIFY(module); watch(module, probe);
    graph.disconnect();
    QVERIFY(graph.wait([&] { return probe.destroyed; }));
    graph.destroy(module); // tracked pointer is already null, never dereferenced
    graph.disconnect();
}

void ConsoleBorrowedCoreLifetimeTests::failedInitializationAfterCoreHooks()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(prepare(graph), qPrintable(graph.failure()));
    const unsigned before = qindaqt_gain_stream_failures();
    auto *module = graph.load(arguments(),
        {.beforeLoad = [] { qindaqt_gain_fail_next_stream_new(); }});
    QVERIFY(module == nullptr);
    QCOMPARE(qindaqt_gain_stream_failures(), before + 1);
    QCOMPARE(pw_context_get_object(wp_core_get_pw_context(graph.core()),
                                   PW_TYPE_INTERFACE_Core), nullptr);
    graph.disconnect(); // actual original impl-destroy failed-init lifetime
}

void ConsoleBorrowedCoreLifetimeTests::repeatedSharedCoreModuleChurn()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(prepare(graph), qPrintable(graph.failure()));
    const auto producer = graph.producer(); QVERIFY(producer.valid());
    for (int index = 0; index < 64; ++index) {
        auto *module = graph.load(arguments()); QVERIFY(module);
        QVERIFY(graph.wait([&] { return graph.nodes().size() >= 2; }));
        graph.destroy(module);
        QVERIFY(graph.wait([&] { return graph.nodes().isEmpty(); }));
        QVERIFY(graph.producer() == producer);
        QCOMPARE(pw_context_get_object(wp_core_get_pw_context(graph.core()),
                                       PW_TYPE_INTERFACE_Core), nullptr);
    }
    graph.disconnect();
}

void ConsoleBorrowedCoreLifetimeTests::moduleOverrideAfterBootstrapRefusesBeforeFactory()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(prepare(graph), qPrintable(graph.failure()));
    const QByteArray previous = qgetenv("PIPEWIRE_MODULE_DIR");
    const auto before = graph.nodes();
    auto *module = graph.load(arguments(), {.beforeLoad = [] {
        qputenv("PIPEWIRE_MODULE_DIR", "/absent-qindaqt-private-module-override");
    }});
    qputenv("PIPEWIRE_MODULE_DIR", previous);
    QVERIFY(module == nullptr);
    QVERIFY(graph.nodes() == before);
    QCOMPARE(pw_context_get_object(wp_core_get_pw_context(graph.core()),
                                   PW_TYPE_INTERFACE_Core), nullptr);
}

QTEST_GUILESS_MAIN(ConsoleBorrowedCoreLifetimeTests)
#include "tst_console_borrowed_core_lifetime.moc"
