// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0179: a strip's rack as one filter-chain. The module argument is a
// string PipeWire parses internally and refuses silently, so its shape - the
// plugin labels, the control names, the block order, the links - is pinned.

#include "../../../src/services/audio_service/src/wireplumber_processing_p.h"

#include <QtTest>

using namespace QindaQt::Audio;

class WirePlumberProcessingTests final : public QObject
{
    Q_OBJECT
private slots:
    void anIdleRackHasNoChain();
    void enabledBlocksAreChainedInOrder();
    void namesAndControlsMatchThePlugins();
    void controlsListOnlyEnabledBlocks();
};

void WirePlumberProcessingTests::anIdleRackHasNoChain()
{
    QVERIFY(!processingActive(StripProcessing{}));
    QVERIFY(processingModuleArguments(QStringLiteral("strip.hw.1"),
                                      QStringLiteral("alsa_input.x"), false,
                                      StripProcessing{})
                .isEmpty());
    QVERIFY(processingControls(StripProcessing{}).isEmpty());
}

void WirePlumberProcessingTests::enabledBlocksAreChainedInOrder()
{
    StripProcessing rack;
    rack.gate.enabled = true;
    rack.equalizer.enabled = true;
    rack.limiter.enabled = true;
    const QString text = QString::fromUtf8(processingModuleArguments(
        QStringLiteral("strip.hw.1"), QStringLiteral("alsa_input.x"), false, rack));
    QVERIFY(!text.isEmpty());
    // gate -> eq_low -> eq_mid -> eq_high -> lim, with the compressor absent.
    QVERIFY(!text.contains(QStringLiteral("name = comp")));
    QVERIFY(text.contains(QStringLiteral("{ output = \"gate:Output\" input = \"eq_low:Input\" }")));
    QVERIFY(text.contains(QStringLiteral("{ output = \"eq_high:Output\" input = \"lim:Input\" }")));
    QVERIFY(text.contains(QStringLiteral("inputs = [ \"gate:Input\" ]")));
    QVERIFY(text.contains(QStringLiteral("outputs = [ \"lim:Output\" ]")));
    // Reads the device, plays into the processed sink the sends will read.
    QVERIFY(text.contains(QStringLiteral("target.object = \"alsa_input.x\"")));
    QVERIFY(text.contains(QStringLiteral("stream.capture.sink = false")));
    QVERIFY(text.contains(QStringLiteral("node.name = \"%1\"")
                              .arg(processedNodeName(QStringLiteral("strip.hw.1")))));
    // An OUTPUT stream cannot be a sink; the processed node is a virtual source.
    QVERIFY(text.contains(QStringLiteral("media.class = Audio/Source")));
    QVERIFY(!text.contains(QStringLiteral("media.class = Audio/Sink")));
    // The chain's capture links to its device or nothing; the processed
    // source is a device and never a stream looking for a target.
    QVERIFY(text.contains(QStringLiteral("node.dont-fallback = true")));
    QCOMPARE(text.count(QStringLiteral("node.autoconnect = false")), 1);

    // A virtual strip is read from its sink's monitor.
    const QString virtualText = QString::fromUtf8(processingModuleArguments(
        QStringLiteral("strip.virtual.1"), QStringLiteral("qindaqt.console.strip.virtual.1"),
        true, rack));
    QVERIFY(virtualText.contains(QStringLiteral("stream.capture.sink = true")));
}

void WirePlumberProcessingTests::namesAndControlsMatchThePlugins()
{
    StripProcessing rack;
    rack.gate.enabled = true;
    rack.gate.thresholdDb = -35.0;
    rack.compressor.enabled = true;
    rack.compressor.ratio = 4.0;
    rack.limiter.enabled = true;
    rack.limiter.releaseMs = 250.0;
    const QString text = QString::fromUtf8(processingModuleArguments(
        QStringLiteral("strip.hw.2"), QStringLiteral("alsa_input.x"), false, rack));
    // swh plugin files and labels, exactly.
    // File basenames carry the LADSPA unique id; labels are the plugins' own.
    // AGENT-GUARD: the MONO variants. The chain runs once per channel, and a
    // stereo plugin (sc4, fastLookaheadLimiter) has no "Input" port, so
    // PipeWire refused the whole graph: "unknown input port comp:Input".
    QVERIFY(text.contains(QStringLiteral("plugin = gate_1410 label = gate")));
    QVERIFY(text.contains(QStringLiteral("plugin = sc4m_1916 label = sc4m")));
    QVERIFY(text.contains(QStringLiteral("plugin = hard_limiter_1413 label = hardLimiter")));
    QVERIFY(text.contains(QStringLiteral("\"Output select (-1 = key listen, 0 = gate, 1 = bypass)\" = 0")));
    // The console's range is wider than sc4's; the graph only sees sc4's.
    StripProcessing deep = rack;
    deep.compressor.thresholdDb = -60.0;
    const QString clamped = QString::fromUtf8(processingModuleArguments(
        QStringLiteral("strip.hw.2"), QStringLiteral("alsa_input.x"), false, deep));
    QVERIFY(clamped.contains(QStringLiteral("\"Threshold level (dB)\" = -30.000")));
    QVERIFY(text.contains(QStringLiteral("\"Threshold (dB)\" = -35.000")));
    QVERIFY(text.contains(QStringLiteral("\"Ratio (1:n)\" = 4.000")));
    // A brick-wall limiter: the ceiling is its only dial.
    QVERIFY(text.contains(QStringLiteral("\"dB limit\" = -1.000")));
    QVERIFY(text.contains(QStringLiteral("\"Wet level\" = 1.0")));
    QVERIFY(processedNodeName(QStringLiteral("strip.hw.2"))
            != processingChainNodeName(QStringLiteral("strip.hw.2")));
    // A hostile device name never reaches the argument.
    QVERIFY(processingModuleArguments(QStringLiteral("strip.hw.2"),
                                      QStringLiteral("evil\" } capture.props = { x = \"y"),
                                      false, rack)
                .isEmpty());
}

void WirePlumberProcessingTests::controlsListOnlyEnabledBlocks()
{
    StripProcessing rack;
    rack.compressor.enabled = true;
    rack.compressor.makeupDb = 2.5;
    const auto controls = processingControls(rack);
    QCOMPARE(controls.size(), 6);
    bool sawMakeup = false;
    for (const auto &[name, value] : controls) {
        QVERIFY(name.startsWith("comp:"));
        sawMakeup = sawMakeup || (name == "comp:Makeup gain (dB)" && value == 2.5);
    }
    QVERIFY(sawMakeup);
}

QTEST_APPLESS_MAIN(WirePlumberProcessingTests)
#include "tst_wireplumber_processing.moc"
