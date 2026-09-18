// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/obs_client/obs_types.h>

#include <QJsonObject>
#include <QString>

#include <optional>

namespace QindaQt::Obs::Protocol {

// The obs-websocket v5 wire, as pure functions. Nothing here owns a socket,
// a timer or state, so every rule below is testable without OBS running.
//
// AGENT-CONTRACT: obs-websocket v5 frames are `{"op": <n>, "d": {...}}`.
// This module encodes and decodes exactly the opcodes the desktop uses and
// refuses everything else, rather than passing an unknown frame through as
// if it had been understood.

// The opcodes this client speaks. Values are obs-websocket's, not ours.
enum class OpCode {
    Hello = 0,
    Identify = 1,
    Identified = 2,
    Reidentify = 3,
    Event = 5,
    Request = 6,
    RequestResponse = 7,
};

// The RPC version this client implements. obs-websocket refuses an Identify
// whose version it cannot serve, and the client reports that as a reason
// code rather than retrying forever.
inline constexpr int RpcVersion = 1;

// Event subscription bits (obs-websocket `EventSubscription`). The desktop
// asks for exactly the categories it renders: a firehose would wake the
// shell for every scene-item move in a busy scene.
namespace EventSubscription {
inline constexpr int General = 1 << 0;
inline constexpr int Config = 1 << 1;
inline constexpr int Scenes = 1 << 2;
inline constexpr int Inputs = 1 << 3;
inline constexpr int Outputs = 1 << 6;
inline constexpr int Vendors = 1 << 10;
inline constexpr int Desktop =
    General | Config | Scenes | Inputs | Outputs | Vendors;
} // namespace EventSubscription

// What a Hello frame asked of us.
struct Hello {
    QString obsWebSocketVersion;
    int rpcVersion = 0;
    // Empty challenge and salt mean the server accepts an unauthenticated
    // Identify; a password supplied anyway is simply unused.
    QString challenge;
    QString salt;

    [[nodiscard]] bool requiresAuthentication() const {
        return !challenge.isEmpty() && !salt.isEmpty();
    }
    friend bool operator==(const Hello &, const Hello &) = default;
};

struct RequestResponse {
    QString requestType;
    QString requestId;
    bool ok = false;
    int code = 0;
    QString comment;
    QJsonObject data;

    friend bool operator==(const RequestResponse &, const RequestResponse &) = default;
};

struct Event {
    QString eventType;
    QJsonObject data;

    friend bool operator==(const Event &, const Event &) = default;
};

// A frame, decoded into exactly one of the shapes this client handles.
struct Frame {
    OpCode op = OpCode::Hello;
    Hello hello;
    RequestResponse response;
    Event event;
    // Identified carries only the negotiated version.
    int negotiatedRpcVersion = 0;
};

// Decodes one text frame. `nullopt` means the text was not a frame this
// client understands — malformed JSON, a missing `op`/`d`, an opcode outside
// the table, or a payload whose required fields are absent or wrongly typed.
// The caller treats that as a protocol fault, never as an empty update.
[[nodiscard]] std::optional<Frame> decodeFrame(const QString &text);

// The obs-websocket v5 authentication string:
//   base64(sha256(base64(sha256(password + salt)) + challenge))
// Returns an empty string for an empty password, which is what an
// unauthenticated Identify sends.
[[nodiscard]] QString authenticationString(const QString &password,
                                           const QString &salt,
                                           const QString &challenge);

// The Identify frame answering `hello`. When the server asked for
// authentication and `password` is empty the result still identifies, so the
// server's own refusal (and not a guess here) produces the reason code.
[[nodiscard]] QString encodeIdentify(const Hello &hello, const QString &password,
                                     int eventSubscriptions =
                                         EventSubscription::Desktop);

// A Request frame. `requestId` correlates the response; the client generates
// it and never reuses one within a connection.
[[nodiscard]] QString encodeRequest(const QString &requestType,
                                    const QString &requestId,
                                    const QJsonObject &requestData = {});

// A vendor request (obs-websocket's `CallVendorRequest`), which is how the
// QindaQt OBS bridge publishes the console mapping.
[[nodiscard]] QString encodeVendorRequest(const QString &vendorName,
                                          const QString &vendorRequestType,
                                          const QString &requestId,
                                          const QJsonObject &requestData = {});

// The payload a vendor response or a VendorEvent carries, unwrapped from its
// envelope. `nullopt` when the envelope is not this vendor's or not a vendor
// payload at all, so another plugin's vendor traffic cannot be read as the
// console mapping.
[[nodiscard]] std::optional<QJsonObject>
unwrapVendorPayload(const QJsonObject &envelope, const QString &vendorName,
                    const QString &expectedType);

// Projections of the response/event payloads this client consumes. Each
// takes the object OBS sent and returns the value type; a missing field
// keeps its default rather than inventing a number.
[[nodiscard]] OutputStatus recordStatusFrom(const QJsonObject &data);
[[nodiscard]] OutputStatus streamStatusFrom(const QJsonObject &data);
[[nodiscard]] OutputStatus virtualCamStatusFrom(const QJsonObject &data);
[[nodiscard]] SceneList sceneListFrom(const QJsonObject &data);
[[nodiscard]] QList<AudioInput> audioInputsFrom(const QJsonObject &data);
[[nodiscard]] ConsoleMapping consoleMappingFrom(const QJsonObject &data);

// The OBS request names this client sends, in one table so a surface never
// spells one itself.
namespace Requests {
inline constexpr char GetVersion[] = "GetVersion";
inline constexpr char GetRecordStatus[] = "GetRecordStatus";
inline constexpr char StartRecord[] = "StartRecord";
inline constexpr char StopRecord[] = "StopRecord";
inline constexpr char GetStreamStatus[] = "GetStreamStatus";
inline constexpr char StartStream[] = "StartStream";
inline constexpr char StopStream[] = "StopStream";
inline constexpr char GetVirtualCamStatus[] = "GetVirtualCamStatus";
inline constexpr char StartVirtualCam[] = "StartVirtualCam";
inline constexpr char StopVirtualCam[] = "StopVirtualCam";
inline constexpr char GetSceneList[] = "GetSceneList";
inline constexpr char SetCurrentProgramScene[] = "SetCurrentProgramScene";
inline constexpr char GetInputList[] = "GetInputList";
inline constexpr char GetInputMute[] = "GetInputMute";
inline constexpr char SetInputMute[] = "SetInputMute";
inline constexpr char CallVendorRequest[] = "CallVendorRequest";
} // namespace Requests

// The OBS event names this client acts on.
namespace Events {
inline constexpr char RecordStateChanged[] = "RecordStateChanged";
inline constexpr char StreamStateChanged[] = "StreamStateChanged";
inline constexpr char VirtualcamStateChanged[] = "VirtualcamStateChanged";
inline constexpr char CurrentProgramSceneChanged[] = "CurrentProgramSceneChanged";
inline constexpr char SceneListChanged[] = "SceneListChanged";
inline constexpr char SceneCreated[] = "SceneCreated";
inline constexpr char SceneRemoved[] = "SceneRemoved";
inline constexpr char SceneNameChanged[] = "SceneNameChanged";
inline constexpr char InputMuteStateChanged[] = "InputMuteStateChanged";
inline constexpr char InputCreated[] = "InputCreated";
inline constexpr char InputRemoved[] = "InputRemoved";
inline constexpr char VendorEvent[] = "VendorEvent";
inline constexpr char ExitStarted[] = "ExitStarted";
} // namespace Events

} // namespace QindaQt::Obs::Protocol
