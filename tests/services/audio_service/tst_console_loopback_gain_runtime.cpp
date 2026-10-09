// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/gain_native_graph.h"
#include "support/gain_pcm_stream.h"
#include "support/gain_private_runtime.h"
#include "wireplumber_routing_p.h"

#include <qindaqt/services/audio_protocol/audio_validation.h>
#include <QtTest>
#include <cmath>
#include <limits>

using namespace QindaQt::Audio;
using namespace QindaQt::Audio::Test;

namespace
{
constexpr auto kSource = "qindaqt.console.vstrip.gain-fixture";
constexpr auto kTarget = "qindaqt.console.vbus.gain-fixture";
bool admit(GainNativeGraph &graph, ConsoleSendAdmission &admission, const quint64 instance)
{
    return graph.wait([&] {
        admission.observe(graph.producer(), graph.nodes());
        return admission.playback(instance).has_value();
    });
}
bool closeTo(const double actual, const double expected)
{
    return std::isfinite(actual) && std::abs(actual - expected)
        <= std::max(0.0000001, std::abs(expected) * 0.015);
}
bool visible(const Snapshot &snapshot, const QString &name)
{
    for (const Device &device : snapshot.outputs) if (device.nodeName == name) return true;
    for (const Device &device : snapshot.inputs) if (device.nodeName == name) return true;
    return false;
}
bool containsSerial(const Snapshot &snapshot, const quint64 serial)
{
    for (const Stream &stream : snapshot.streams) if (stream.handle.serial == serial) return true;
    return false;
}
}

class ConsoleLoopbackGainRuntimeTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void actualPropsAndFirstPcmStayQuietWhileSetterDelayed();
    void savedLoudRestoreControlAndQuietReplacement();
    void independentGainAndCubicPan_data();
    void independentGainAndCubicPan();
    void copiedIdentityFromDifferentClientStaysPublic();
    void duplicateRoleAndSerialReplacementNeverGrantWrites();
    void refusedSetterDoesNotClaimRunningGain();
};
void ConsoleLoopbackGainRuntimeTests::actualPropsAndFirstPcmStayQuietWhileSetterDelayed()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    QVERIFY(graph.createSink(QString::fromLatin1(kSource)));
    QVERIFY(graph.createSink(QString::fromLatin1(kTarget)));
    GainPcmStream generator, capture;
    QVERIFY(generator.start(wp_core_get_pw_core(graph.core()), QString::fromLatin1(kSource),
                            GainPcmStream::Direction::Produce));
    QVERIFY(capture.start(wp_core_get_pw_core(graph.core()), QString::fromLatin1(kTarget),
                          GainPcmStream::Direction::ObserveSink));
    QVERIFY(graph.wait([&] { return generator.streaming() && capture.streaming()
                                   && capture.window().frames > 1024; }));
    capture.resetWindow();
    ConsoleSendAdmission admission; admission.reset(graph.producer());
    const auto declaration = admission.reserve(routingNodeName(QStringLiteral("fixture"), QStringLiteral("bus")));
    QVERIFY(declaration);
    auto *module = graph.load(ownedRoutingModuleArguments(*declaration, QStringLiteral("fixture"),
        QStringLiteral("bus"), QString::fromLatin1(kSource), QString::fromLatin1(kTarget), true));
    QVERIFY(module); QVERIFY(admit(graph, admission, declaration->instance));
    QCOMPARE(pw_context_get_object(wp_core_get_pw_context(graph.core()), PW_TYPE_INTERFACE_Core), nullptr);
    const auto node = admission.playback(declaration->instance); QVERIFY(node);
    QVERIFY(graph.wait([&] { return graph.props(*node).has_value(); }));
    auto initial = graph.props(*node); QVERIFY(initial);
    QVERIFY(initial->muted); QCOMPARE(initial->volumes[0], 0.0F); QCOMPARE(initial->volumes[1], 0.0F);
    // Include every block since module creation, with a deliberate delay before
    // any mixer setter. Frame evidence is mandatory for a silence assertion.
    graph.settle(400);
    const auto quiet = capture.window(); QVERIFY(quiet.frames > 4096);
    QVERIFY(quiet.peak[0] <= 0.0000001F && quiet.peak[1] <= 0.0000001F);
    const auto built = graph.snapshot(&admission);
    QVERIFY(!built.truncatedOrMalformed); QVERIFY(validateSnapshot(built.snapshot).accepted);
    QVERIFY(visible(built.snapshot, QString::fromLatin1(kSource)));
    QVERIFY(visible(built.snapshot, QString::fromLatin1(kTarget)));
    QVERIFY(!containsSerial(built.snapshot, node->serial));
    const auto unity = consoleSendAmplitude({});
    QVERIFY(graph.setGain(*node, unity));
    QVERIFY(graph.wait([&] { const auto props = graph.props(*node);
        return props && !props->muted && closeTo(props->volumes[0], 1.0)
            && closeTo(props->volumes[1], 1.0); }));
    graph.settle(150); capture.resetWindow(); graph.settle(250);
    const auto audible = capture.window(); QVERIFY(audible.frames > 4096);
    QVERIFY(closeTo(audible.peak[0], GainPcmStream::kInputAmplitude));
    QVERIFY(closeTo(audible.peak[1], GainPcmStream::kInputAmplitude));
}
void ConsoleLoopbackGainRuntimeTests::savedLoudRestoreControlAndQuietReplacement()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(true), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    QVERIFY(graph.createSink(QString::fromLatin1(kSource)));
    QVERIFY(graph.createSink(QString::fromLatin1(kTarget)));
    GainPcmStream generator, capture;
    QVERIFY(generator.start(wp_core_get_pw_core(graph.core()), QString::fromLatin1(kSource),
                            GainPcmStream::Direction::Produce));
    QVERIFY(capture.start(wp_core_get_pw_core(graph.core()), QString::fromLatin1(kTarget),
                          GainPcmStream::Direction::ObserveSink));
    auto identify = [](QByteArray arguments) {
        return arguments.replace("playback.props = {",
            "playback.props = { application.id = qindaqt.gain-quiet-test");
    };
    // Real policy-restored loud control proves the seed/rule path is active.
    auto *legacy = graph.load(identify(routingModuleArguments(QStringLiteral("legacy"),
        QStringLiteral("bus"), QString::fromLatin1(kSource), QString::fromLatin1(kTarget), true, 1.0)));
    QVERIFY(legacy);
    QVERIFY(graph.wait([&] {
        return capture.streaming() && closeTo(capture.window().peak[0], 0.00025)
            && closeTo(capture.window().peak[1], 0.0005);
    }));
    graph.settle(150); capture.resetWindow(); graph.settle(250);
    QVERIFY(capture.window().frames > 4096);
    QVERIFY(closeTo(capture.window().peak[0], 0.00025));
    QVERIFY(closeTo(capture.window().peak[1], 0.0005));
    graph.destroy(legacy);
    QVERIFY(graph.wait([&] {
        return !WirePlumberGraph::findNodeByName(graph.manager(),
            routingNodeName(QStringLiteral("legacy"), QStringLiteral("bus"))).has_value();
    }));
    graph.settle(150); capture.resetWindow(); graph.settle(150);
    QVERIFY(capture.window().frames > 4096);
    QVERIFY(capture.window().peak[0] <= 0.0000001F);

    ConsoleSendAdmission admission; admission.reset(graph.producer());
    const auto declaration = admission.reserve(routingNodeName(QStringLiteral("fixture"), QStringLiteral("bus")));
    QVERIFY(declaration);
    capture.resetWindow();
    auto *quiet = graph.load(identify(ownedRoutingModuleArguments(*declaration, QStringLiteral("fixture"),
        QStringLiteral("bus"), QString::fromLatin1(kSource), QString::fromLatin1(kTarget), true)));
    QVERIFY(quiet); QVERIFY(admit(graph, admission, declaration->instance));
    const auto node = admission.playback(declaration->instance); QVERIFY(node);
    graph.settle(400);
    const auto props = graph.props(*node); QVERIFY(props); QVERIFY(props->muted);
    QCOMPARE(props->volumes[0], 0.0F); QCOMPARE(props->volumes[1], 0.0F);
    const auto pcm = capture.window(); QVERIFY(pcm.frames > 4096);
    QVERIFY(pcm.peak[0] <= 0.0000001F && pcm.peak[1] <= 0.0000001F);
}
void ConsoleLoopbackGainRuntimeTests::independentGainAndCubicPan_data()
{
    QTest::addColumn<double>("strip"); QTest::addColumn<double>("send");
    QTest::addColumn<double>("bus"); QTest::addColumn<double>("pan");
    QTest::addColumn<bool>("audible");
    QTest::newRow("strip-minus6") << -6.0 << 0.0 << 0.0 << 0.0 << true;
    QTest::newRow("send-minus6") << 0.0 << -6.0 << 0.0 << 0.0 << true;
    QTest::newRow("bus-minus6") << 0.0 << 0.0 << -6.0 << 0.0 << true;
    QTest::newRow("pan-right-half") << 0.0 << 0.0 << 0.0 << 0.5 << true;
    QTest::newRow("boost-plus12") << 12.0 << 0.0 << 0.0 << 0.0 << true;
    QTest::newRow("boost-plus36") << 12.0 << 12.0 << 12.0 << 0.0 << true;
    QTest::newRow("strip-bottom-with-boost") << -60.0 << 12.0 << 12.0 << 0.0 << true;
    QTest::newRow("send-bottom-with-boost") << 12.0 << -60.0 << 12.0 << 0.0 << true;
    QTest::newRow("bus-bottom-with-boost") << 12.0 << 12.0 << -60.0 << 0.0 << true;
    QTest::newRow("mute-or-excluded-by-solo") << 12.0 << 12.0 << 12.0 << 0.0 << false;
}
void ConsoleLoopbackGainRuntimeTests::independentGainAndCubicPan()
{
    QFETCH(double, strip); QFETCH(double, send); QFETCH(double, bus);
    QFETCH(double, pan); QFETCH(bool, audible);
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    QVERIFY(graph.createSink(QString::fromLatin1(kSource)));
    QVERIFY(graph.createSink(QString::fromLatin1(kTarget)));
    ConsoleSendAdmission admission; admission.reset(graph.producer());
    const auto declaration = admission.reserve(routingNodeName(QStringLiteral("fixture"), QStringLiteral("bus")));
    QVERIFY(declaration);
    auto *module = graph.load(ownedRoutingModuleArguments(*declaration, QStringLiteral("fixture"),
        QStringLiteral("bus"), QString::fromLatin1(kSource), QString::fromLatin1(kTarget), true));
    QVERIFY(module); QVERIFY(admit(graph, admission, declaration->instance));
    const auto node = admission.playback(declaration->instance); QVERIFY(node);
    GainPcmStream generator, capture;
    QVERIFY(generator.start(wp_core_get_pw_core(graph.core()), QString::fromLatin1(kSource),
                            GainPcmStream::Direction::Produce));
    QVERIFY(capture.start(wp_core_get_pw_core(graph.core()), QString::fromLatin1(kTarget),
                          GainPcmStream::Direction::ObserveSink));
    const auto amplitude = consoleSendAmplitude({strip, send, bus, pan, audible});
    QVERIFY(amplitude.valid); QVERIFY(graph.setGain(*node, amplitude));
    QVERIFY(graph.wait([&] { const auto current = graph.props(*node);
        return current && !current->muted && closeTo(current->volumes[0], amplitude.left)
            && closeTo(current->volumes[1], amplitude.right); }));
    QVERIFY(graph.wait([&] { return capture.streaming() && generator.streaming(); }));
    graph.settle(150); capture.resetWindow(); graph.settle(250);
    const auto pcm = capture.window(); QVERIFY(pcm.frames > 4096);
    QVERIFY(closeTo(pcm.peak[0], GainPcmStream::kInputAmplitude * amplitude.left));
    QVERIFY(closeTo(pcm.peak[1], GainPcmStream::kInputAmplitude * amplitude.right));
    const auto ordinary = graph.snapshot(&admission);
    QVERIFY(!ordinary.truncatedOrMalformed);
    QVERIFY(validateSnapshot(ordinary.snapshot).accepted);
    QVERIFY(visible(ordinary.snapshot, QString::fromLatin1(kSource)));
    QVERIFY(visible(ordinary.snapshot, QString::fromLatin1(kTarget)));
}
void ConsoleLoopbackGainRuntimeTests::copiedIdentityFromDifferentClientStaysPublic()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    QVERIFY(graph.createSink(QString::fromLatin1(kSource)));
    QVERIFY(graph.createSink(QString::fromLatin1(kTarget)));
    ConsoleSendAdmission admission; admission.reset(graph.producer());
    const auto declaration = admission.reserve(routingNodeName(QStringLiteral("fixture"), QStringLiteral("bus")));
    QVERIFY(declaration);
    GainNativeGraph foreign(runtime); QVERIFY2(foreign.start(), qPrintable(foreign.failure()));
    QVERIFY(foreign.producer().clientId != graph.producer().clientId);
    // Same process/PID and full name/token/role tuple; only the actual daemon
    // client differs. No payload PID can grant ownership.
    const auto arguments = ownedRoutingModuleArguments(*declaration, QStringLiteral("fixture"),
        QStringLiteral("bus"), QString::fromLatin1(kSource), QString::fromLatin1(kTarget), true);
    auto *module = foreign.load(arguments); QVERIFY(module);
    QVERIFY(graph.wait([&] { return graph.nodes().size() >= 2; }));
    admission.observe(graph.producer(), graph.nodes());
    QVERIFY(!admission.playback(declaration->instance));
    const auto built = graph.snapshot(&admission);
    QVERIFY(!built.truncatedOrMalformed); QVERIFY(validateSnapshot(built.snapshot).accepted);
    bool copiedPlaybackVisible = false;
    for (const auto &node : graph.nodes()) if (node.role == QLatin1String("playback")) {
        QVERIFY(node.clientId == foreign.producer().clientId);
        QVERIFY(!admission.excludes(node.serial, node.objectIdentity));
        copiedPlaybackVisible |= containsSerial(built.snapshot, node.serial);
    }
    QVERIFY(copiedPlaybackVisible);
}
void ConsoleLoopbackGainRuntimeTests::duplicateRoleAndSerialReplacementNeverGrantWrites()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    QVERIFY(graph.createSink(QString::fromLatin1(kSource)));
    QVERIFY(graph.createSink(QString::fromLatin1(kTarget)));
    ConsoleSendAdmission admission; admission.reset(graph.producer());
    const auto declaration = admission.reserve(routingNodeName(QStringLiteral("fixture"), QStringLiteral("bus")));
    QVERIFY(declaration);
    const auto arguments = ownedRoutingModuleArguments(*declaration, QStringLiteral("fixture"),
        QStringLiteral("bus"), QString::fromLatin1(kSource), QString::fromLatin1(kTarget), true);
    auto *first = graph.load(arguments); QVERIFY(first);
    QVERIFY(admit(graph, admission, declaration->instance));
    const auto old = admission.playback(declaration->instance); QVERIFY(old);
    auto *duplicate = graph.load(arguments); QVERIFY(duplicate);
    QVERIFY(graph.wait([&] { return graph.nodes().size() >= 4; }));
    admission.observe(graph.producer(), graph.nodes());
    QVERIFY(!admission.playback(declaration->instance));
    QVERIFY(admission.invalidInstances().contains(declaration->instance));
    admission.retire(declaration->instance);
    graph.destroy(first); graph.destroy(duplicate);
    QVERIFY(graph.wait([&] { admission.observe(graph.producer(), graph.nodes());
                             return admission.size() == 0; }));
    auto *replacement = graph.load(arguments); QVERIFY(replacement);
    QVERIFY(graph.wait([&] { return graph.nodes().size() >= 2; }));
    admission.observe(graph.producer(), graph.nodes());
    QVERIFY(!admission.playback(declaration->instance));
    for (const auto &node : graph.nodes()) {
        QVERIFY(node.serial != old->serial);
        QVERIFY(!admission.excludes(node.serial, node.objectIdentity));
    }
    graph.destroy(replacement);
}
void ConsoleLoopbackGainRuntimeTests::refusedSetterDoesNotClaimRunningGain()
{
    PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
    GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
    QVERIFY(!graph.setById(std::numeric_limits<quint32>::max(), consoleSendAmplitude({})));
    graph.disconnect();
    QVERIFY(!graph.setById(graph.producer().clientId, consoleSendAmplitude({})));
    ConsoleSendAdmission admission; admission.reset(graph.producer());
    QVERIFY(!admission.reserve(routingNodeName(QStringLiteral("fixture"), QStringLiteral("bus"))));
}
QTEST_GUILESS_MAIN(ConsoleLoopbackGainRuntimeTests)
#include "tst_console_loopback_gain_runtime.moc"
