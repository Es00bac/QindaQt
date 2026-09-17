// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_protocol/wiz_types.h>

#include <QtCore/QByteArray>

#include <optional>

namespace QindaQt::Wiz
{

// The vendor's JSON-RPC-shaped methods that this implementation speaks.
enum class Method : quint32 {
    Unknown = 0,
    GetPilot = 1,
    SetPilot = 2,
    // Unsolicited state notification a registered luminaire pushes on change.
    SyncPilot = 3,
    Registration = 4,
    GetSystemConfig = 5,
    GetModelConfig = 6,
};

struct SystemConfigPayload {
    QString mac;
    QString moduleName;
    QString firmwareVersion;
    quint32 homeId = 0;
    quint32 roomId = 0;

    friend bool operator==(const SystemConfigPayload &, const SystemConfigPayload &) = default;
};

// The subset of getModelConfig this implementation trusts. Everything else the
// firmware reports describes LED driver electronics and is deliberately
// ignored.
struct ModelConfigPayload {
    bool temperatureRangeKnown = false;
    int minimumKelvin = 0;
    int maximumKelvin = 0;
    bool dimmingFloorKnown = false;
    int minimumDimmingPercent = 0;
    // Independently controllable heads. Two means an up/down luminaire whose
    // warm/cool ratio can be set.
    int headCount = 1;

    friend bool operator==(const ModelConfigPayload &, const ModelConfigPayload &) = default;
};

// One decoded datagram. A message carries at most one payload; the `*Known`
// bits say which.
//
// AGENT-GUARD: decoding never fabricates a field. A member the datagram did
// not contain keeps its default and its `known` bit stays false, so a missing
// value can never be mistaken for zero.
struct DecodedMessage {
    Method method = Method::Unknown;
    QString mac;
    bool hasError = false;
    int errorCode = 0;
    QString errorMessage;
    // True when the device acknowledged a mutation (setPilot, registration).
    bool acknowledged = false;
    // True when the payload arrived as `params`: a notification the device
    // pushed on its own, not a reply to something this host sent.
    //
    // AGENT-GUARD (verified on firmware 1.38.0): a push leaves the luminaire
    // from an ephemeral source port (51501 and 59321 were observed), not from
    // the control port it listens on. Nothing may learn routing from where a
    // push came from; WizModel::observe is where that rule is enforced.
    bool unsolicited = false;
    bool pilotKnown = false;
    PilotState pilot;
    bool systemConfigKnown = false;
    SystemConfigPayload systemConfig;
    bool modelConfigKnown = false;
    ModelConfigPayload modelConfig;

    friend bool operator==(const DecodedMessage &, const DecodedMessage &) = default;
};

[[nodiscard]] QByteArray encodeGetPilot();
[[nodiscard]] QByteArray encodeGetSystemConfig();
[[nodiscard]] QByteArray encodeGetModelConfig();

// Asks the luminaire to push syncPilot notifications to `listenerAddress`.
// `register` false is also the vendor's unauthenticated discovery ping: every
// light on the broadcast domain answers it with its MAC.
[[nodiscard]] QByteArray encodeRegistration(const QString &listenerAddress,
                                            const QString &listenerMac,
                                            bool subscribe);

// Encodes exactly the fields whose `set*` bit is true. The caller is expected
// to have passed `request` through validateStateRequest() first; encoding does
// not repair an inconsistent request, it only refuses an empty one.
[[nodiscard]] QByteArray encodeSetPilot(const StateRequest &request);

// Rejects anything larger than Limits::maximumDatagramBytes, not-JSON, or
// missing a method name, without allocating unbounded intermediate state.
[[nodiscard]] std::optional<DecodedMessage> decodeMessage(const QByteArray &datagram);

[[nodiscard]] Method methodFromString(const QString &value);

} // namespace QindaQt::Wiz
