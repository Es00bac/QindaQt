// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
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

struct Console {
    QList<Strip> strips = {};
    QList<Bus> buses = {};
    // True when at least one strip is soloed. Published rather than derived so
    // every surface agrees on the state that silences un-soloed strips, and so
    // a client that shows only part of the console still dims correctly.
    bool soloActive = false;

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
Q_DECLARE_METATYPE(QindaQt::Audio::Strip)
Q_DECLARE_METATYPE(QindaQt::Audio::Bus)
Q_DECLARE_METATYPE(QindaQt::Audio::Console)
