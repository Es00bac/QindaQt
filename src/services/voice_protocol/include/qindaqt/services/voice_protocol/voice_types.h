// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_protocol/voice_limits.h>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Services::Voice {

// AGENT-CONTRACT: org.qindaqt.Voice1 is owned by QindaQt and implemented by a
// replaceable provider process. Gabbee's implementation lives in its own
// repository at src/gabbee/qindaqt_voice.py; both sides are bound by
// docs/wiki/architecture/voice-input.md and must move together.
inline constexpr char kServiceName[] = "org.qindaqt.Voice1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Voice1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Voice1";

// Consumer-side lifecycle of a VoiceClient. Distinct from SessionState, which
// is the provider's own capture state.
enum class ClientState : quint32 {
    Stopped,
    Starting,
    Ready,
    Unavailable,
};

// The provider's capture state. Unknown exists only so an out-of-range wire
// value decodes to something nameable before validation rejects the snapshot;
// a conforming provider never reports it.
enum class SessionState : quint32 {
    Unknown = 0,
    Idle,
    Arming,
    Listening,
    Transcribing,
    Delivering,
    Error,
};

enum class CaptureMode : quint32 {
    Dictation = 0,
    Command,
};

// Which delivery path placed the last final text. The shell renders this as
// provenance only; it never selects a route.
enum class DeliveryRoute : quint32 {
    None = 0,
    InputMethod,
    Accessibility,
    Clipboard,
    KeySynthesis,
};

enum class OperationKind : quint32 {
    StartDictation = 0,
    StartCommand,
    Finish,
    Cancel,
    Retry,
    Undo,
    CopyLast,
    SetProvider,
    SetEnabled,
};

enum class OperationStatus : quint32 {
    Succeeded = 0,
    Rejected,
    Failed,
    Uncertain,
    Busy,
};

// What the attached provider can actually do, reported by the provider rather
// than assumed from its identity. The applet disables a control whose bit is
// clear instead of letting the user submit an intent that must fail.
enum Capability : quint32 {
    CapabilityNone = 0,
    CapabilityCommandMode = 1u << 0,
    CapabilityRealtimePartials = 1u << 1,
    CapabilityRetry = 1u << 2,
    CapabilityUndo = 1u << 3,
    CapabilityCopy = 1u << 4,
    CapabilityProviderSelection = 1u << 5,
};

inline constexpr quint32 kKnownCapabilities =
    CapabilityCommandMode | CapabilityRealtimePartials | CapabilityRetry
    | CapabilityUndo | CapabilityCopy | CapabilityProviderSelection;

inline constexpr SessionState kMaxSessionState = SessionState::Error;
inline constexpr CaptureMode kMaxCaptureMode = CaptureMode::Command;
inline constexpr DeliveryRoute kMaxDeliveryRoute = DeliveryRoute::KeySynthesis;
inline constexpr OperationKind kMaxOperationKind = OperationKind::SetEnabled;
inline constexpr OperationStatus kMaxOperationStatus = OperationStatus::Busy;

// One speech provider the user could switch to.
//
// AGENT-CONTRACT: `available` is the provider's own answer to "would this work
// right now" — a credential present, a model downloaded. The shell renders an
// unavailable entry but never dispatches to it, so a provider must not omit
// one merely because it is not configured yet.
struct ProviderDescriptor {
    QString id;
    QString label;
    bool available = false;

    friend bool operator==(const ProviderDescriptor &, const ProviderDescriptor &) = default;
};

// The complete caller-visible truth of the voice input stack at one revision.
//
// AGENT-GUARD: revision is the only ordering authority. A provider advances it
// when, and only when, a field of this struct changed. Microphone level is
// deliberately absent: it changes at frame rate, carries no decision, and is
// delivered by an unrevisioned signal so a meter cannot churn the projection.
struct Snapshot {
    quint32 schemaVersion = kSchemaVersion;
    quint64 revision = 0;
    SessionState state = SessionState::Unknown;
    CaptureMode mode = CaptureMode::Dictation;
    bool enabled = false;
    quint32 capabilities = CapabilityNone;
    DeliveryRoute lastRoute = DeliveryRoute::None;
    QString providerId;
    QString providerLabel;
    QString languageCode;
    QString microphoneLabel;
    QString dictationShortcut;
    QString commandShortcut;
    QString partialText;
    QString lastText;
    QString reasonCode;
    QList<ProviderDescriptor> providers;
    bool wireValid = true;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

struct OperationRequest {
    OperationKind kind = OperationKind::StartDictation;
    quint64 requestId = 0;
    quint64 expectedRevision = 0;
    // Only SetProvider reads this; only SetEnabled reads enable.
    QString providerId;
    bool enable = false;

    friend bool operator==(const OperationRequest &, const OperationRequest &) = default;
};

struct OperationResult {
    OperationKind kind = OperationKind::StartDictation;
    OperationStatus status = OperationStatus::Failed;
    quint64 requestId = 0;
    quint64 initiatingRevision = 0;
    quint64 observedRevision = 0;
    QString reasonCode;
    bool wireValid = true;

    friend bool operator==(const OperationResult &, const OperationResult &) = default;
};

[[nodiscard]] bool capabilityForKind(OperationKind kind, quint32 capabilities) noexcept;

} // namespace QindaQt::Services::Voice

Q_DECLARE_METATYPE(QindaQt::Services::Voice::ClientState)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::SessionState)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::CaptureMode)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::DeliveryRoute)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::OperationKind)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::OperationStatus)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::ProviderDescriptor)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::OperationRequest)
Q_DECLARE_METATYPE(QindaQt::Services::Voice::OperationResult)
