// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>

namespace QindaQt::Audio
{

// AGENT-CONTRACT: the mixing-console slice of Audio1 (ADR-0123 S4, ADR-0173).
// This is the model a Voicemeeter-class console is actually made of, and it is
// deliberately NOT expressible in terms of the device/stream slice: a console
// has INPUT STRIPS that each route to SEVERAL OUTPUT BUSES at once, with an
// independent gain on every strip-to-bus link. Devices and streams describe
// "what exists"; strips and buses describe "how the user has wired it".
//
// Every level in this model is decibels through the one gain law in
// audio_gain.h. Nothing here carries a linear scalar.

// Where a strip's audio comes from. A hardware strip follows a capture device;
// a virtual strip is a managed null sink that applications select as an output,
// which is how one application gets its own fader.
enum class StripKind : quint32 {
    HardwareInput = 0,
    VirtualInput = 1,
};

// Where a bus sends audio. A physical bus drives a real output device (the
// reference console's A1-A5); a virtual bus is a managed null sink other
// applications can record from (B1-B3).
enum class BusKind : quint32 {
    Physical = 0,
    Virtual = 1,
};

// One meter reading. Both values are dBFS (0 dB is full scale, negative below);
// this is NOT the fader scale in audio_gain.h and must never be fed through the
// fader taper. `known` is false until the service has observed real audio, so a
// console draws an idle meter instead of a misleading silence.
struct Level {
    double peakDb = kSilentMeterDb;
    double rmsDb = kSilentMeterDb;
    bool known = false;

    friend bool operator==(const Level &, const Level &) = default;
};

// One cell of the routing matrix: this strip's send to one bus.
//
// AGENT-GUARD: `enabled` and `gainDb` are independent on purpose. Turning a
// send off must not destroy the level the user dialled in, so a re-enabled send
// comes back where it was rather than at unity.
struct MatrixSend {
    quint32 busIndex = 0;
    bool enabled = false;
    double gainDb = 0.0;

    friend bool operator==(const MatrixSend &, const MatrixSend &) = default;
};

// The per-strip processing rack (ADR-0179): what the reference console puts
// between the input and the fader. Each block is independently switchable and
// keeps its settings while off, so turning a compressor off and on again
// restores the user's dial positions rather than defaults. Times are
// milliseconds, levels dBFS, gains dB; the graph applies them in this order:
// gate, compressor, equalizer, limiter.
// The denoiser (ADR-0180): RNNoise, first in the chain so the gate and the
// compressor act on the cleaned signal. `vadThreshold` is the voice-activity
// confidence, 0-100 %, below which the plugin mutes the frame outright.
struct DenoiserSettings {
    bool enabled = false;
    double vadThreshold = 50.0;

    friend bool operator==(const DenoiserSettings &, const DenoiserSettings &) = default;
};

struct GateSettings {
    bool enabled = false;
    double thresholdDb = -40.0;
    double attackMs = 5.0;
    double holdMs = 50.0;
    double releaseMs = 200.0;
    // How far the gate closes, in dB below unity; -90 is effectively silence.
    double rangeDb = -60.0;

    friend bool operator==(const GateSettings &, const GateSettings &) = default;
};

struct CompressorSettings {
    bool enabled = false;
    double thresholdDb = -18.0;
    double ratio = 3.0;
    double attackMs = 10.0;
    double releaseMs = 100.0;
    double kneeDb = 6.0;
    double makeupDb = 0.0;

    friend bool operator==(const CompressorSettings &, const CompressorSettings &) = default;
};

struct LimiterSettings {
    bool enabled = false;
    double ceilingDb = -1.0;
    double releaseMs = 50.0;

    friend bool operator==(const LimiterSettings &, const LimiterSettings &) = default;
};

// Three bands like the reference console's strip EQ: a low shelf, a peaking
// mid with its own width, and a high shelf.
struct EqualizerSettings {
    bool enabled = false;
    double lowHz = 120.0;
    double lowGainDb = 0.0;
    double midHz = 1000.0;
    double midGainDb = 0.0;
    double midQ = 1.0;
    double highHz = 8000.0;
    double highGainDb = 0.0;

    friend bool operator==(const EqualizerSettings &, const EqualizerSettings &) = default;
};

struct StripProcessing {
    DenoiserSettings denoiser;
    GateSettings gate;
    CompressorSettings compressor;
    EqualizerSettings equalizer;
    LimiterSettings limiter;

    friend bool operator==(const StripProcessing &, const StripProcessing &) = default;
};

// What a bus can do to its mix on the way out (ADR-0180): a three-band
// equalizer and a channel mode. The modes are the stereo-desktop subset of the
// reference console's bus modes; the surround upmixes are not modelled.
enum class BusMode : quint32 {
    Normal = 0,
    SwapChannels = 1,
    LeftToBoth = 2,
    RightToBoth = 3,
};

struct BusProcessing {
    EqualizerSettings equalizer;
    BusMode mode = BusMode::Normal;

    friend bool operator==(const BusProcessing &, const BusProcessing &) = default;
};

struct Strip {
    // Stable console identity, independent of the graph node behind it: a
    // strip keeps its fader and routing when its device disappears and comes
    // back. Never a PipeWire id.
    QString id;
    StripKind kind = StripKind::HardwareInput;
    // Console position, 0-based and dense within the strip's kind.
    quint32 index = 0;
    QString label;
    // The device this strip follows. `sourceKnown` is false when the device is
    // absent, which leaves the strip visible and configurable rather than
    // silently dropping the user's routing.
    quint64 sourceEpoch = 0;
    quint64 sourceSerial = 0;
    bool sourceKnown = false;

    double gainDb = 0.0;
    bool muted = false;
    bool soloed = false;
    bool mono = false;
    // -1.0 hard left, 0.0 centre, +1.0 hard right.
    double pan = 0.0;
    // Per-channel trim in dB, applied before the fader; empty when the strip's
    // channel layout is not yet known.
    QVector<double> channelTrimDb = {};
    // One entry per bus the console publishes, in bus index order.
    QList<MatrixSend> sends = {};
    Level level;
    // The user's explicit choice of device, as a node.name (ADR-0178). Empty
    // means automatic: the service binds the strip to whatever hardware is
    // available. A pin is honoured even when the device is absent - the strip
    // then stays unbound rather than quietly following another microphone.
    QString pinnedSource = {};
    // The rack (ADR-0179). Appended last so the fields before it keep their
    // wire order.
    StripProcessing processing = {};

    bool wireValid = true;

    friend bool operator==(const Strip &, const Strip &) = default;
};

struct Bus {
    QString id;
    BusKind kind = BusKind::Physical;
    quint32 index = 0;
    QString label;
    // The device this bus drives.
    quint64 targetEpoch = 0;
    quint64 targetSerial = 0;
    bool targetKnown = false;

    double gainDb = 0.0;
    bool muted = false;
    bool mono = false;
    Level level;
    // As Strip::pinnedSource, for the output the bus drives.
    QString pinnedTarget = {};
    // The bus rack (ADR-0180). Appended last.
    BusProcessing processing = {};

    bool wireValid = true;

    friend bool operator==(const Bus &, const Bus &) = default;
};

// One console element's meter, addressed by console id (ADR-0173).
//
// AGENT-CONTRACT: meters move at frame rate; snapshots do not. A snapshot is
// the console's CONFIGURATION and is republished only when the user or the
// graph changes something, while readings stream on their own signal many
// times a second. Folding meters into snapshot republication would mean
// rebuilding and revalidating the whole console dozens of times a second and
// would make every revision number meaningless.
struct LevelReading {
    // A strip or bus id from the same console. A reading naming neither is
    // dropped by the client rather than creating an element.
    QString id;
    Level level;

    friend bool operator==(const LevelReading &, const LevelReading &) = default;
};

// What the recorder is doing (ADR-0184). One recording at a time, of one bus,
// to one file. `startedAtMs` is wall-clock epoch milliseconds so a surface can
// show elapsed time without the service republishing every second.
struct Recording {
    bool active = false;
    QString busId;
    QString path;
    quint64 startedAtMs = 0;

    friend bool operator==(const Recording &, const Recording &) = default;
};

// One VBAN stream as the console knows it (ADR-0185). Outgoing: a bus sent to
// `host:port` under `name`. Incoming: packets named `name` arriving on `port`,
// presented as a virtual source. `enabled` is the user's switch; `active` is
// whether the graph currently carries it.
struct VbanStream {
    QString name;
    bool outgoing = true;
    QString busId;
    QString host;
    quint32 port = 6980;
    bool enabled = false;
    bool active = false;

    friend bool operator==(const VbanStream &, const VbanStream &) = default;
};

struct Console {
    QList<Strip> strips = {};
    QList<Bus> buses = {};
    // True when at least one strip is soloed. Published rather than derived so
    // every surface agrees on the state that silences un-soloed strips, and so
    // a client that shows only part of the console still dims correctly.
    bool soloActive = false;
    // The names of the saved presets (ADR-0182), in stored order. Published so
    // every surface offers the same list without a second round trip.
    QStringList presets = {};
    // The names of the macro buttons (ADR-0183), in file order.
    QStringList macros = {};
    Recording recording = {};
    QList<VbanStream> vban = {};

    bool wireValid = true;

    friend bool operator==(const Console &, const Console &) = default;
};

} // namespace QindaQt::Audio

Q_DECLARE_METATYPE(QindaQt::Audio::StripKind)
Q_DECLARE_METATYPE(QindaQt::Audio::BusKind)
Q_DECLARE_METATYPE(QindaQt::Audio::Level)
Q_DECLARE_METATYPE(QindaQt::Audio::MatrixSend)
Q_DECLARE_METATYPE(QindaQt::Audio::LevelReading)
Q_DECLARE_METATYPE(QList<QindaQt::Audio::LevelReading>)
Q_DECLARE_METATYPE(QindaQt::Audio::DenoiserSettings)
Q_DECLARE_METATYPE(QindaQt::Audio::BusMode)
Q_DECLARE_METATYPE(QindaQt::Audio::BusProcessing)
Q_DECLARE_METATYPE(QindaQt::Audio::GateSettings)
Q_DECLARE_METATYPE(QindaQt::Audio::CompressorSettings)
Q_DECLARE_METATYPE(QindaQt::Audio::LimiterSettings)
Q_DECLARE_METATYPE(QindaQt::Audio::EqualizerSettings)
Q_DECLARE_METATYPE(QindaQt::Audio::StripProcessing)
Q_DECLARE_METATYPE(QindaQt::Audio::Strip)
Q_DECLARE_METATYPE(QindaQt::Audio::Bus)
Q_DECLARE_METATYPE(QindaQt::Audio::Recording)
Q_DECLARE_METATYPE(QindaQt::Audio::VbanStream)
Q_DECLARE_METATYPE(QindaQt::Audio::Console)
