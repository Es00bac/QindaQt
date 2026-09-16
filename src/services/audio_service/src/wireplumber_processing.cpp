// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wireplumber_processing_p.h"

#include "console_endpoints_p.h"
#include "wireplumber_routing_p.h"

#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QStringList>

namespace QindaQt::Audio
{
namespace {

// swh-plugins file names, labels and control names (LADSPA), verified with
// `analyseplugin` on swh-plugins 0.4.17. The file name carries the plugin's
// unique id; `plugin =` is that basename without the extension.
// noise-suppression-for-voice (RNNoise) LADSPA: one mono suppressor per
// channel, like the rest of the chain.
constexpr char kDenoiserPlugin[] = "librnnoise_ladspa";
constexpr char kDenoiserLabel[] = "noise_suppressor_mono";
constexpr char kGatePlugin[] = "gate_1410";
constexpr char kGateLabel[] = "gate";
// The MONO variants on purpose: the chain is instantiated once per channel
// (see processingModuleArguments), so every plugin must expose exactly one
// "Input" and one "Output". sc4 and the lookahead limiter are stereo and
// have no such ports; PipeWire refuses the whole graph on the first link.
constexpr char kCompressorPlugin[] = "sc4m_1916";
constexpr char kCompressorLabel[] = "sc4m";
constexpr char kLimiterPlugin[] = "hard_limiter_1413";
constexpr char kLimiterLabel[] = "hardLimiter";

QString number(const double value)
{
    return QString::number(value, 'f', 3);
}

// AGENT-GUARD: the wire's bounds are the console's; each plugin has its own,
// narrower range and behaves unpredictably outside it (sc4 accepts no
// threshold below -30 dB, the gate no hold under 2 ms). Values are clamped
// to the plugin here, so the console can offer one consistent scale and the
// graph still only ever sees what the plugin was written for.
double clampTo(const double value, const double low, const double high)
{
    return value < low ? low : (value > high ? high : value);
}

QString denoiserNode(const DenoiserSettings &denoiser)
{
    // VAD threshold in percent; the grace periods are left at the plugin's
    // defaults, which are tuned for speech.
    return QStringLiteral(
               "{ type = ladspa name = denoise plugin = %1 label = %2 control = {"
               " \"VAD Threshold (%)\" = %3 } }")
        .arg(QLatin1String(kDenoiserPlugin), QLatin1String(kDenoiserLabel),
             number(clampTo(denoiser.vadThreshold, 0.0, 99.0)));
}

QString gateNode(const GateSettings &gate)
{
    // Output select 0 = gated signal. The key filters are left wide open; a
    // sidechain filter is a later block.
    return QStringLiteral(
               "{ type = ladspa name = gate plugin = %1 label = %2 control = {"
               " \"LF key filter (Hz)\" = 30.0 \"HF key filter (Hz)\" = 20000.0"
               " \"Threshold (dB)\" = %3 \"Attack (ms)\" = %4 \"Hold (ms)\" = %5"
               " \"Decay (ms)\" = %6 \"Range (dB)\" = %7"
               " \"Output select (-1 = key listen, 0 = gate, 1 = bypass)\" = 0 } }")
        .arg(QLatin1String(kGatePlugin), QLatin1String(kGateLabel),
             number(clampTo(gate.thresholdDb, -70.0, 20.0)),
             number(clampTo(gate.attackMs, 0.01, 1000.0)),
             number(clampTo(gate.holdMs, 2.0, 2000.0)),
             number(clampTo(gate.releaseMs, 2.0, 4000.0)),
             number(clampTo(gate.rangeDb, -90.0, 0.0)));
}

QString compressorNode(const CompressorSettings &compressor)
{
    return QStringLiteral(
               "{ type = ladspa name = comp plugin = %1 label = %2 control = {"
               " \"RMS/peak\" = 0.0 \"Attack time (ms)\" = %3 \"Release time (ms)\" = %4"
               " \"Threshold level (dB)\" = %5 \"Ratio (1:n)\" = %6"
               " \"Knee radius (dB)\" = %7 \"Makeup gain (dB)\" = %8 } }")
        .arg(QLatin1String(kCompressorPlugin), QLatin1String(kCompressorLabel),
             number(clampTo(compressor.attackMs, 1.5, 400.0)),
             number(clampTo(compressor.releaseMs, 2.0, 800.0)),
             number(clampTo(compressor.thresholdDb, -30.0, 0.0)),
             number(clampTo(compressor.ratio, 1.0, 20.0)),
             number(clampTo(compressor.kneeDb, 1.0, 10.0)),
             number(clampTo(compressor.makeupDb, 0.0, 24.0)));
}

QString equalizerNodes(const EqualizerSettings &eq)
{
    // Three builtin biquads: PipeWire's bq_* take Freq, Q and Gain.
    return QStringLiteral(
               "{ type = builtin name = eq_low label = bq_lowshelf control = {"
               " \"Freq\" = %1 \"Q\" = 0.707 \"Gain\" = %2 } }"
               " { type = builtin name = eq_mid label = bq_peaking control = {"
               " \"Freq\" = %3 \"Q\" = %4 \"Gain\" = %5 } }"
               " { type = builtin name = eq_high label = bq_highshelf control = {"
               " \"Freq\" = %6 \"Q\" = 0.707 \"Gain\" = %7 } }")
        .arg(number(eq.lowHz), number(eq.lowGainDb), number(eq.midHz), number(eq.midQ),
             number(eq.midGainDb), number(eq.highHz), number(eq.highGainDb));
}

QString limiterNode(const LimiterSettings &limiter)
{
    // A brick-wall limiter: fully wet, no residue. It has no release control;
    // the wire's releaseMs is kept for a lookahead limiter with one, and is
    // simply not applied here.
    return QStringLiteral(
               "{ type = ladspa name = lim plugin = %1 label = %2 control = {"
               " \"dB limit\" = %3 \"Wet level\" = 1.0 \"Residue level\" = 0.0 } }")
        .arg(QLatin1String(kLimiterPlugin), QLatin1String(kLimiterLabel),
             number(clampTo(limiter.ceilingDb, -50.0, 0.0)));
}

} // namespace

QString processedNodeName(const QString &stripId)
{
    return QLatin1String(ConsoleEndpoints::kConsoleNodeNamePrefix) + stripId
        + QStringLiteral(".processed");
}

QString processingChainNodeName(const QString &stripId)
{
    return QLatin1String(ConsoleEndpoints::kConsoleNodeNamePrefix) + stripId
        + QStringLiteral(".rack");
}

bool processingActive(const StripProcessing &p)
{
    return stripProcessingActive(p);
}

QByteArray processingModuleArguments(const QString &stripId, const QString &sourceNodeName,
                                     const bool sourceIsSink,
                                     const StripProcessing &processing)
{
    const QString chain = processingChainNodeName(stripId);
    const QString processed = processedNodeName(stripId);
    if (!routingNameIsEmbeddable(chain) || !routingNameIsEmbeddable(processed)
        || !routingNameIsEmbeddable(sourceNodeName) || !processingActive(processing)) {
        return {};
    }
    // Enabled blocks in processing order; the chain links each to the next.
    QStringList nodes;
    QStringList order;
    if (processing.denoiser.enabled) {
        nodes << denoiserNode(processing.denoiser);
        order << QStringLiteral("denoise");
    }
    if (processing.gate.enabled) {
        nodes << gateNode(processing.gate);
        order << QStringLiteral("gate");
    }
    if (processing.compressor.enabled) {
        nodes << compressorNode(processing.compressor);
        order << QStringLiteral("comp");
    }
    if (processing.equalizer.enabled) {
        nodes << equalizerNodes(processing.equalizer);
        order << QStringLiteral("eq_low") << QStringLiteral("eq_mid")
              << QStringLiteral("eq_high");
    }
    if (processing.limiter.enabled) {
        nodes << limiterNode(processing.limiter);
        order << QStringLiteral("lim");
    }
    QStringList links;
    for (qsizetype index = 0; index + 1 < order.size(); ++index) {
        links << QStringLiteral("{ output = \"%1:Output\" input = \"%2:Input\" }")
                     .arg(order.at(index), order.at(index + 1));
    }
    // AGENT-GUARD: the swh plugins are mono; PipeWire runs one chain instance
    // per channel when the stream is stereo, so a stereo strip is processed
    // as two identical mono chains. Gate and compressor therefore act per
    // channel, which for a microphone strip is the reference behaviour.
    return QStringLiteral(
               "{ node.name = \"%1\" node.description = \"QindaQt rack %2\""
               " capture.props = { node.name = \"%1.capture\" target.object = \"%3\""
               " stream.capture.sink = %4 node.dont-fallback = true audio.position = [ FL FR ] }"
               // AGENT-GUARD: the playback side is an OUTPUT stream, and PipeWire
               // refuses "Audio/Sink" on one ("does not expect Output stream
               // direction"). As Audio/Source it is a virtual source the sends
               // and the meter read like any capture device.
               " playback.props = { node.name = \"%5\" media.class = Audio/Source"
               " audio.position = [ FL FR ] node.autoconnect = false }"
               " filter.graph = { nodes = [ %6 ] links = [ %7 ]"
               " inputs = [ \"%8:Input\" ] outputs = [ \"%9:Output\" ] } }")
        .arg(chain, stripId, sourceNodeName,
             sourceIsSink ? QStringLiteral("true") : QStringLiteral("false"), processed,
             nodes.join(QLatin1Char(' ')), links.join(QLatin1Char(' ')), order.first(),
             order.last())
        .toUtf8();
}

QString busSinkNodeNameFor(const QString &busId)
{
    return ConsoleEndpoints::busSinkNodeName(busId);
}

QString busChainNodeName(const QString &busId)
{
    return QLatin1String(ConsoleEndpoints::kConsoleNodeNamePrefix) + busId
        + QStringLiteral(".rack");
}


QByteArray busProcessingModuleArguments(const QString &busId, const QString &deviceNodeName,
                                        const BusProcessing &processing)
{
    const QString chain = busChainNodeName(busId);
    const QString sink = busSinkNodeNameFor(busId);
    if (!routingNameIsEmbeddable(chain) || !routingNameIsEmbeddable(sink)
        || !routingNameIsEmbeddable(deviceNodeName) || !busProcessingActive(processing)) {
        return {};
    }
    // AGENT-CONTRACT: a bus rack is an explicit STEREO graph - one equalizer
    // per channel - because the channel modes are about which channel goes
    // where, which a per-channel duplicated graph cannot express. Left is
    // channel 1, right channel 2. An equalizer that is off still exists with
    // its gains at zero: the mode alone can need the chain.
    const EqualizerSettings &eq = processing.equalizer;
    const double lowGain = eq.enabled ? eq.lowGainDb : 0.0;
    const double midGain = eq.enabled ? eq.midGainDb : 0.0;
    const double highGain = eq.enabled ? eq.highGainDb : 0.0;
    QStringList nodes;
    QStringList links;
    for (const QString &side : {QStringLiteral("l"), QStringLiteral("r")}) {
        nodes << QStringLiteral(
                     "{ type = builtin name = eq_%1_low label = bq_lowshelf control = {"
                     " \"Freq\" = %2 \"Q\" = 0.707 \"Gain\" = %3 } }"
                     " { type = builtin name = eq_%1_mid label = bq_peaking control = {"
                     " \"Freq\" = %4 \"Q\" = %5 \"Gain\" = %6 } }"
                     " { type = builtin name = eq_%1_high label = bq_highshelf control = {"
                     " \"Freq\" = %7 \"Q\" = 0.707 \"Gain\" = %8 } }")
                     .arg(side, number(eq.lowHz), number(lowGain), number(eq.midHz),
                          number(eq.midQ), number(midGain), number(eq.highHz),
                          number(highGain));
        links << QStringLiteral("{ output = \"eq_%1_low:Out\" input = \"eq_%1_mid:In\" }"
                                " { output = \"eq_%1_mid:Out\" input = \"eq_%1_high:In\" }")
                     .arg(side);
    }
    // The mode is the output port mapping: which chain end feeds which
    // channel of the device.
    QString outputs;
    switch (processing.mode) {
    case BusMode::Normal:
        outputs = QStringLiteral("\"eq_l_high:Out\" \"eq_r_high:Out\"");
        break;
    case BusMode::SwapChannels:
        outputs = QStringLiteral("\"eq_r_high:Out\" \"eq_l_high:Out\"");
        break;
    case BusMode::LeftToBoth:
        outputs = QStringLiteral("\"eq_l_high:Out\" \"eq_l_high:Out\"");
        break;
    case BusMode::RightToBoth:
        outputs = QStringLiteral("\"eq_r_high:Out\" \"eq_r_high:Out\"");
        break;
    }
    return QStringLiteral(
               "{ node.name = \"%1\" node.description = \"QindaQt bus rack %2\""
               " capture.props = { node.name = \"%1.capture\" target.object = \"%3\""
               " stream.capture.sink = true node.dont-fallback = true audio.position = [ FL FR ] }"
               " playback.props = { node.name = \"%1.playback\" target.object = \"%4\""
               " node.dont-fallback = true audio.position = [ FL FR ] }"
               " filter.graph = { nodes = [ %5 ] links = [ %6 ]"
               " inputs = [ \"eq_l_low:In\" \"eq_r_low:In\" ] outputs = [ %7 ] } }")
        .arg(chain, busId, sink, deviceNodeName, nodes.join(QLatin1Char(' ')),
             links.join(QLatin1Char(' ')), outputs)
        .toUtf8();
}

QList<QPair<QByteArray, double>> processingControls(const StripProcessing &p)
{
    QList<QPair<QByteArray, double>> controls;
    if (p.denoiser.enabled) {
        controls << qMakePair(QByteArray("denoise:VAD Threshold (%)"), p.denoiser.vadThreshold);
    }
    if (p.gate.enabled) {
        controls << qMakePair(QByteArray("gate:Threshold (dB)"), p.gate.thresholdDb)
                 << qMakePair(QByteArray("gate:Attack (ms)"), p.gate.attackMs)
                 << qMakePair(QByteArray("gate:Hold (ms)"), p.gate.holdMs)
                 << qMakePair(QByteArray("gate:Decay (ms)"), p.gate.releaseMs)
                 << qMakePair(QByteArray("gate:Range (dB)"), p.gate.rangeDb);
    }
    if (p.compressor.enabled) {
        controls << qMakePair(QByteArray("comp:Attack time (ms)"), p.compressor.attackMs)
                 << qMakePair(QByteArray("comp:Release time (ms)"), p.compressor.releaseMs)
                 << qMakePair(QByteArray("comp:Threshold level (dB)"), p.compressor.thresholdDb)
                 << qMakePair(QByteArray("comp:Ratio (1:n)"), p.compressor.ratio)
                 << qMakePair(QByteArray("comp:Knee radius (dB)"), p.compressor.kneeDb)
                 << qMakePair(QByteArray("comp:Makeup gain (dB)"), p.compressor.makeupDb);
    }
    if (p.equalizer.enabled) {
        controls << qMakePair(QByteArray("eq_low:Freq"), p.equalizer.lowHz)
                 << qMakePair(QByteArray("eq_low:Gain"), p.equalizer.lowGainDb)
                 << qMakePair(QByteArray("eq_mid:Freq"), p.equalizer.midHz)
                 << qMakePair(QByteArray("eq_mid:Q"), p.equalizer.midQ)
                 << qMakePair(QByteArray("eq_mid:Gain"), p.equalizer.midGainDb)
                 << qMakePair(QByteArray("eq_high:Freq"), p.equalizer.highHz)
                 << qMakePair(QByteArray("eq_high:Gain"), p.equalizer.highGainDb);
    }
    if (p.limiter.enabled) {
        controls << qMakePair(QByteArray("lim:dB limit"), p.limiter.ceilingDb);
    }
    return controls;
}

} // namespace QindaQt::Audio
