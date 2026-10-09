// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/gain_native_graph.h"
#include "support/gain_private_runtime.h"
#include "wireplumber_routing_p.h"

#include <pipewire/impl-module.h>
#include <QtTest>
#include <cerrno>

using namespace QindaQt::Audio;
using namespace QindaQt::Audio::Test;

namespace
{
int refuseInsert(pw_context *context, const char *key, void *value)
{
    return value ? -ENOMEM : pw_context_set_object(context, key, nullptr);
}
struct DestroyProbe {
    pw_context *context = nullptr; spa_hook *listener = nullptr;
    bool observed = false; bool empty = false;
};
void observeDestroy(void *data)
{
    auto *probe = static_cast<DestroyProbe *>(data);
    if (probe->listener) { spa_hook_remove(probe->listener); probe->listener = nullptr; }
    probe->observed = true;
    probe->empty = pw_context_get_object(probe->context, PW_TYPE_INTERFACE_Core) == nullptr;
}
struct NestedProbe {
    WpCore *core = nullptr; bool refused = false; bool outerPreserved = false;
    spa_hook *listener = nullptr;
};
void attemptNested(void *data)
{
    auto *probe = static_cast<NestedProbe *>(data);
    // Unlink while the module's hook list still exists, never after free.
    if (probe->listener) { spa_hook_remove(probe->listener); probe->listener = nullptr; }
    ScopedContextCore nested(probe->core);
    probe->refused = !nested.admitted();
    probe->outerPreserved = pw_context_get_object(wp_core_get_pw_context(probe->core),
        PW_TYPE_INTERFACE_Core) == wp_core_get_pw_core(probe->core);
}
}

class ConsoleCoreScopeRuntimeTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void missingImageAdmissionRefusesBeforeSlotInsertion();
    void occupiedSameAndOtherPointerRemainUntouched();
    void failedInsertionAndFailedFactoryRemoveOnlyOwnSlot();
    void nestedModuleDestroyCannotBorrowOuterSlot();
    void disconnectBeforeFactoryRevokesAndClearsBeforeFinalFree();
    void stopAndImmediateReconnectCannotReuseProducer();
};
void ConsoleCoreScopeRuntimeTests::missingImageAdmissionRefusesBeforeSlotInsertion()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    auto *context = wp_core_get_pw_context(graph.core());
    bool factoryHookReached = false;
    const QByteArray arguments = QByteArrayLiteral("{ audio.channels = 2 }");
    QVERIFY(loadOwnedLoopback(graph.core(), arguments) == nullptr);
    QVERIFY(loadOwnedLoopback(graph.core(), arguments,
        {.admitModule = [] { return false; },
         .beforeLoad = [&] { factoryHookReached = true; }}) == nullptr);
    QVERIFY(!factoryHookReached);
    QCOMPARE(pw_context_get_object(context, PW_TYPE_INTERFACE_Core), nullptr);
}
void ConsoleCoreScopeRuntimeTests::occupiedSameAndOtherPointerRemainUntouched()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    auto *context = wp_core_get_pw_context(graph.core());
    auto *raw = wp_core_get_pw_core(graph.core());
    for (void *occupant : {static_cast<void *>(raw), static_cast<void *>(&runtime)}) {
        QCOMPARE(pw_context_set_object(context, PW_TYPE_INTERFACE_Core, occupant), 0);
        { ScopedContextCore scope(graph.core()); QVERIFY(!scope.admitted()); }
        QCOMPARE(pw_context_get_object(context, PW_TYPE_INTERFACE_Core), occupant);
        QCOMPARE(pw_context_set_object(context, PW_TYPE_INTERFACE_Core, nullptr), 0);
    }
    {
        ScopedContextCore outer(graph.core()); QVERIFY(outer.admitted());
        { ScopedContextCore nested(graph.core()); QVERIFY(!nested.admitted()); }
        QVERIFY(outer.admitted());
        outer.release(); outer.release(); QVERIFY(!outer.admitted());
    }
    QCOMPARE(pw_context_get_object(context, PW_TYPE_INTERFACE_Core), nullptr);
}
void ConsoleCoreScopeRuntimeTests::failedInsertionAndFailedFactoryRemoveOnlyOwnSlot()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    auto *context = wp_core_get_pw_context(graph.core());
    { ScopedContextCore denied(graph.core(), {&pw_context_get_object, &refuseInsert});
      QVERIFY(!denied.admitted()); }
    QCOMPARE(pw_context_get_object(context, PW_TYPE_INTERFACE_Core), nullptr);
    // A real fixed factory failure, not an argument-string assertion.
    QVERIFY(graph.load(QByteArrayLiteral("{ audio.channels = 999 }")) == nullptr);
    QCOMPARE(pw_context_get_object(context, PW_TYPE_INTERFACE_Core), nullptr);
    {
        ScopedContextCore owned(graph.core()); QVERIFY(owned.admitted());
        QCOMPARE(pw_context_set_object(context, PW_TYPE_INTERFACE_Core, &runtime), 0);
        QVERIFY(!owned.admitted()); owned.release();
        QCOMPARE(pw_context_get_object(context, PW_TYPE_INTERFACE_Core), &runtime);
        QCOMPARE(pw_context_set_object(context, PW_TYPE_INTERFACE_Core, nullptr), 0);
    }
}
void ConsoleCoreScopeRuntimeTests::nestedModuleDestroyCannotBorrowOuterSlot()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    NestedProbe probe{};
    spa_hook listener{};
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    QVERIFY(graph.createSink(QStringLiteral("gain.scope.source")));
    QVERIFY(graph.createSink(QStringLiteral("gain.scope.target")));
    auto *module = graph.load(routingModuleArguments(QStringLiteral("scope"), QStringLiteral("bus"),
        QStringLiteral("gain.scope.source"), QStringLiteral("gain.scope.target"), true, 1.0));
    QVERIFY(module);
    probe.core = graph.core();
    static const pw_impl_module_events events = [] {
        pw_impl_module_events value{};
        value.version = PW_VERSION_IMPL_MODULE_EVENTS; value.destroy = attemptNested;
        return value;
    }();
    probe.listener = &listener;
    pw_impl_module_add_listener(module, &listener, &events, &probe);
    {
        ScopedContextCore outer(graph.core()); QVERIFY(outer.admitted());
        graph.destroy(module); // synchronous destruction callback tries another scope
        QVERIFY(probe.refused); QVERIFY(probe.outerPreserved); QVERIFY(outer.admitted());
    }
    QVERIFY(probe.listener == nullptr);
    QCOMPARE(pw_context_get_object(wp_core_get_pw_context(graph.core()), PW_TYPE_INTERFACE_Core), nullptr);
}
void ConsoleCoreScopeRuntimeTests::disconnectBeforeFactoryRevokesAndClearsBeforeFinalFree()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    DestroyProbe probe;
    spa_hook listener{};
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    auto *context = wp_core_get_pw_context(graph.core());
    {
        ScopedContextCore scope(graph.core()); QVERIFY(scope.admitted());
        probe.context = context; probe.listener = &listener;
        static const pw_proxy_events events = [] {
            pw_proxy_events value{}; value.version = PW_VERSION_PROXY_EVENTS;
            value.destroy = observeDestroy; return value;
        }();
        auto *raw = wp_core_get_pw_core(graph.core());
        pw_proxy_add_listener(reinterpret_cast<pw_proxy *>(raw), &listener, &events, &probe);
        graph.disconnect();
        QVERIFY(probe.observed); QVERIFY(probe.empty); QVERIFY(!scope.admitted());
        QVERIFY(probe.listener == nullptr);
        scope.release(); scope.release();
    }
    QCOMPARE(pw_context_get_object(context, PW_TYPE_INTERFACE_Core), nullptr);
    // New Core: force loss while registration is active but before the fixed
    // synchronous factory. This cannot produce a module or dispatch a loop.
    GainNativeGraph reconnect(runtime); QVERIFY2(reconnect.start(), qPrintable(reconnect.failure()));
    auto *module = reconnect.load(QByteArrayLiteral("{ node.name = gain.scope.denied }"),
        {.beforeLoad = [&] { reconnect.disconnect(); }});
    QVERIFY(!module);
    QCOMPARE(pw_context_get_object(wp_core_get_pw_context(reconnect.core()), PW_TYPE_INTERFACE_Core), nullptr);
    // This probe proves slot removal before final proxy memory free, not before
    // PipeWire's earlier internal teardown. ADR0361 records that exact ordering.
}
void ConsoleCoreScopeRuntimeTests::stopAndImmediateReconnectCannotReuseProducer()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    SendProducerIdentity old;
    ConsoleSendAdmission admission;
    {
        GainNativeGraph first(runtime); QVERIFY2(first.start(), qPrintable(first.failure()));
        old = first.producer(1); QVERIFY(old.valid()); admission.reset(old);
        QVERIFY(admission.reserve(routingNodeName(QStringLiteral("a"), QStringLiteral("b"))));
        first.disconnect(); first.disconnect();
        admission.observe(first.producer(1), first.nodes());
        QCOMPARE(admission.size(), 0);
    }
    GainNativeGraph next(runtime); QVERIFY2(next.start(), qPrintable(next.failure()));
    const auto current = next.producer(2); QVERIFY(current.valid()); QVERIFY(current != old);
    admission.reset(current); QCOMPARE(admission.size(), 0);
    QVERIFY(admission.reserve(routingNodeName(QStringLiteral("a"), QStringLiteral("b"))));
}
QTEST_GUILESS_MAIN(ConsoleCoreScopeRuntimeTests)
#include "tst_console_core_scope_runtime.moc"
