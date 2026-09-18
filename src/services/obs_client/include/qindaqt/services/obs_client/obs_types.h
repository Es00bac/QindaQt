// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace QindaQt::Obs {

// What the client knows about its connection to OBS.
//
// AGENT-CONTRACT: `Degraded` means OBS answered and then something went
// wrong (a refused identify, an unsupported RPC version, a malformed frame);
// `Disconnected` means there is nothing to talk to. A surface must be able to
// tell "OBS is not running" from "OBS is running and rejected us", because
// the user's next action is different.
enum class ConnectionState {
    Disconnected,
    Connecting,
    Authenticating,
    Ready,
    Degraded,
};

// Why the client is not Ready. Stable identifiers; the surface maps them to
// sentences, and an unmatched code is shown verbatim rather than hidden.
namespace ReasonCodes {
inline constexpr char None[] = "";
inline constexpr char NotRunning[] = "obs-not-running";
inline constexpr char AuthRequired[] = "obs-auth-required";
inline constexpr char AuthRejected[] = "obs-auth-rejected";
inline constexpr char RpcVersion[] = "obs-rpc-version-unsupported";
inline constexpr char Malformed[] = "obs-malformed-frame";
inline constexpr char Closed[] = "obs-closed-connection";
} // namespace ReasonCodes

// One of the three things OBS can be doing with an output.
enum class OutputKind { Record, Stream, VirtualCam };

// The live state of one output.
//
// `active` is the only field every kind reports. A field OBS does not send
// for a kind keeps its default rather than a guessed value: the Streaming
// route shows a dash, not a zero that looks measured.
struct OutputStatus {
    bool active = false;
    bool paused = false;
    // OBS's own "reconnecting" state for a stream; never inferred.
    bool reconnecting = false;
    // Elapsed time as OBS reports it, in milliseconds. -1 means unknown.
    qint64 durationMs = -1;
    // Frames OBS dropped on this output, and the total it has handled.
    // -1 means OBS did not report the number.
    qint64 skippedFrames = -1;
    qint64 totalFrames = -1;

    [[nodiscard]] bool hasFrameCounts() const {
        return skippedFrames >= 0 && totalFrames > 0;
    }
    // Dropped fraction in 0..1, or -1 when OBS reported no counts. A surface
    // must not present a warning it cannot substantiate.
    [[nodiscard]] double droppedFraction() const {
        return hasFrameCounts() ? double(skippedFrames) / double(totalFrames)
                                : -1.0;
    }

    friend bool operator==(const OutputStatus &, const OutputStatus &) = default;
};

// The scenes OBS holds and which one is live.
struct SceneList {
    QStringList names;
    QString currentProgramScene;
    QString currentPreviewScene;

    friend bool operator==(const SceneList &, const SceneList &) = default;
};

// One audio input OBS holds, with the mute state the top bar mirrors.
struct AudioInput {
    QString name;
    QString inputKind;
    bool muted = false;

    friend bool operator==(const AudioInput &, const AudioInput &) = default;
};

// One console endpoint as the QindaQt OBS bridge publishes it.
//
// AGENT-CONTRACT: These fields are F3's vendor payload, not this client's
// invention (`src/obs/module/websocket_vendor.cpp` at `bd8527a0`). The bus
// mapping table reads them through the vendor request; it never parses an
// OBS source name to work out which bus a source is, because the source name
// is a display string the user can see change.
struct ConsoleSourceMapping {
    QString consoleId;
    QString code;
    QString label;
    QString sourceName;
    // "qindaqt_console_bus" or "qindaqt_console_strip", as the bridge spells
    // its two registered source types.
    QString sourceKind;
    QString captureKind;
    QString captureDevice;
    bool muted = false;
    double gainDb = 0.0;

    friend bool operator==(const ConsoleSourceMapping &,
                           const ConsoleSourceMapping &) = default;
};

// The whole console mapping one vendor reply carries.
struct ConsoleMapping {
    // 0 means no bridge answered yet; the route says the plugin is missing
    // rather than showing an empty table as if the console had no buses.
    int bridgeVersion = 0;
    QList<ConsoleSourceMapping> buses;
    QList<ConsoleSourceMapping> strips;
    // The bridge's view of Audio1, so the route can say "the console is
    // unavailable" instead of blaming OBS.
    QString audioState;
    QString reasonCode;
    // The bridge sends epoch and revision as integers (F3's
    // `obs_data_set_int`), so they are read as integers here; a string would
    // silently become 0.
    quint64 epoch = 0;
    quint64 revision = 0;

    [[nodiscard]] bool present() const { return bridgeVersion > 0; }
    [[nodiscard]] qsizetype size() const {
        return buses.size() + strips.size();
    }

    friend bool operator==(const ConsoleMapping &, const ConsoleMapping &) = default;
};

// Everything a surface needs in one value, so a popup binds to one snapshot
// instead of six properties that can disagree mid-update.
struct ObsSnapshot {
    ConnectionState state = ConnectionState::Disconnected;
    QString reasonCode = QString::fromLatin1(ReasonCodes::NotRunning);
    QString obsVersion;
    QString webSocketVersion;
    OutputStatus record;
    OutputStatus stream;
    OutputStatus virtualCam;
    SceneList scenes;
    QList<AudioInput> audioInputs;
    ConsoleMapping consoleMapping;

    [[nodiscard]] bool ready() const {
        return state == ConnectionState::Ready;
    }
    friend bool operator==(const ObsSnapshot &, const ObsSnapshot &) = default;
};

// AGENT-GUARD: deliberately not named toString. QTest's genericToString
// finds a same-named free function by ADL and then fails to compile every
// QCOMPARE of these enums, because the return type is not const char *.
[[nodiscard]] QString connectionStateName(ConnectionState state);
[[nodiscard]] QString outputKindName(OutputKind kind);

} // namespace QindaQt::Obs

Q_DECLARE_METATYPE(QindaQt::Obs::ConnectionState)
Q_DECLARE_METATYPE(QindaQt::Obs::OutputStatus)
Q_DECLARE_METATYPE(QindaQt::Obs::SceneList)
Q_DECLARE_METATYPE(QindaQt::Obs::ConsoleMapping)
Q_DECLARE_METATYPE(QindaQt::Obs::ObsSnapshot)
