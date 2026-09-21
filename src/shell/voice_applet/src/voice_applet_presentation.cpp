// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/voice_applet/voice_applet_presentation.h>

#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <QtCore/QCoreApplication>

#include <array>

namespace QindaQt::Shell::VoiceApplet {
namespace {

using Services::Voice::CaptureMode;
using Services::Voice::ClientState;
using Services::Voice::DeliveryRoute;
using Services::Voice::OperationKind;
using Services::Voice::SessionState;
using Services::Voice::Snapshot;

struct ActionToken {
    OperationKind kind;
    const char *id;
};

// AGENT-GUARD: ids are a contract with VoiceApplet.qml and with the settings
// page's action list. Renaming one silently disconnects a button.
constexpr std::array kActionTokens{
    ActionToken{OperationKind::StartDictation, "dictate"},
    ActionToken{OperationKind::StartCommand, "command"},
    ActionToken{OperationKind::Finish, "finish"},
    ActionToken{OperationKind::Cancel, "cancel"},
    ActionToken{OperationKind::Retry, "retry"},
    ActionToken{OperationKind::Undo, "undo"},
    ActionToken{OperationKind::CopyLast, "copy"},
    ActionToken{OperationKind::SetProvider, "set-provider"},
    ActionToken{OperationKind::SetEnabled, "set-enabled"},
};

QString translate(const char *text)
{
    return QCoreApplication::translate("VoiceApplet", text);
}

bool isCapturing(const SessionState state)
{
    return state == SessionState::Arming || state == SessionState::Listening
           || state == SessionState::Transcribing;
}

QString stateIdFor(const SessionState state)
{
    switch (state) {
    case SessionState::Idle:         return QStringLiteral("idle");
    case SessionState::Arming:       return QStringLiteral("arming");
    case SessionState::Listening:    return QStringLiteral("listening");
    case SessionState::Transcribing: return QStringLiteral("transcribing");
    case SessionState::Delivering:   return QStringLiteral("delivering");
    case SessionState::Error:        return QStringLiteral("error");
    case SessionState::Unknown:      break;
    }
    return QStringLiteral("unavailable");
}

QString statusLabelFor(const SessionState state, const CaptureMode mode)
{
    const bool command = mode == CaptureMode::Command;
    switch (state) {
    case SessionState::Idle:
        return translate("Ready to dictate");
    case SessionState::Arming:
        return command ? translate("Opening the microphone for a command")
                       : translate("Opening the microphone");
    case SessionState::Listening:
        return command ? translate("Listening for a command") : translate("Listening");
    case SessionState::Transcribing:
        return translate("Transcribing");
    case SessionState::Delivering:
        return command ? translate("Running the command") : translate("Inserting text");
    case SessionState::Error:
        return translate("Voice input failed");
    case SessionState::Unknown:
        break;
    }
    return translate("Voice input is unavailable");
}

QString iconNameFor(const SessionState state, const bool enabled)
{
    if (!enabled) {
        return QStringLiteral("audio-input-microphone-muted");
    }
    switch (state) {
    case SessionState::Arming:
    case SessionState::Listening:
    case SessionState::Transcribing:
    case SessionState::Delivering:
        return QStringLiteral("audio-input-microphone-high");
    case SessionState::Idle:
        return QStringLiteral("audio-input-microphone");
    case SessionState::Error:
    case SessionState::Unknown:
        break;
    }
    return QStringLiteral("audio-input-microphone-muted");
}

QString routeLabelFor(const DeliveryRoute route)
{
    switch (route) {
    case DeliveryRoute::InputMethod:   return translate("Input method");
    case DeliveryRoute::Accessibility: return translate("Accessibility");
    case DeliveryRoute::Clipboard:     return translate("Clipboard paste");
    case DeliveryRoute::KeySynthesis:  return translate("Synthesised keys");
    case DeliveryRoute::None:          break;
    }
    return {};
}

// The chip has room for a word or two. Capture state wins over provenance,
// because the chip is how the user knows the microphone is open.
QString summaryLabelFor(const VoiceAppletModel &model, const SessionState state)
{
    if (!model.readGranted) {
        return translate("Voice");
    }
    if (model.phase != ServicePhase::Ready) {
        return model.phase == ServicePhase::Starting ? translate("Voice…")
                                                     : translate("Voice off");
    }
    switch (state) {
    case SessionState::Arming:       return translate("Arming");
    case SessionState::Listening:    return model.commandMode ? translate("Command")
                                                              : translate("Listening");
    case SessionState::Transcribing: return translate("Transcribing");
    case SessionState::Delivering:   return translate("Inserting");
    case SessionState::Error:        return translate("Voice error");
    default:                         break;
    }
    return model.enabled ? translate("Voice") : translate("Voice off");
}

QString diagnosticFor(const ClientState clientState, const QString &clientReasonCode,
                      const bool hasSnapshot, const Snapshot &snapshot)
{
    if (clientState == ClientState::Ready && hasSnapshot) {
        if (snapshot.state != SessionState::Error) {
            return {};
        }
        // The provider's own reason code is the most specific truth available;
        // it is structured, so it is shown verbatim rather than guessed at.
        return translate("The voice provider reported: %1").arg(snapshot.reasonCode);
    }
    if (clientState == ClientState::Starting) {
        return translate("Connecting to the voice provider.");
    }
    if (clientReasonCode == QLatin1String("service-unavailable")) {
        return translate("No voice provider is running. Install and enable one to "
                         "dictate.");
    }
    if (clientReasonCode.isEmpty()) {
        return translate("Voice input is not connected.");
    }
    return translate("Voice input is unavailable: %1").arg(clientReasonCode);
}

void appendAction(QList<ActionModel> &actions, const OperationKind kind,
                  const QString &label, const QString &iconName, const bool enabled)
{
    actions.append(ActionModel{.id = actionIdForKind(kind),
                               .label = label,
                               .iconName = iconName,
                               .enabled = enabled});
}

void buildActions(VoiceAppletModel &model, const Snapshot &snapshot)
{
    const bool live = model.phase == ServicePhase::Ready && model.controlGranted;
    const bool capturing = model.capturing;
    const quint32 caps = snapshot.capabilities;
    const auto offers = [caps](const OperationKind kind) {
        return Services::Voice::capabilityForKind(kind, caps);
    };

    appendAction(model.actions, OperationKind::StartDictation, translate("Dictate"),
                 QStringLiteral("audio-input-microphone"),
                 live && model.enabled && !capturing);
    if (offers(OperationKind::StartCommand)) {
        appendAction(model.actions, OperationKind::StartCommand, translate("Command"),
                     QStringLiteral("system-run"), live && model.enabled && !capturing);
    }
    appendAction(model.actions, OperationKind::Finish, translate("Finish"),
                 QStringLiteral("emblem-default"), live && capturing);
    appendAction(model.actions, OperationKind::Cancel, translate("Cancel"),
                 QStringLiteral("window-close"), live && capturing);
    if (offers(OperationKind::Retry)) {
        appendAction(model.actions, OperationKind::Retry, translate("Retry"),
                     QStringLiteral("view-refresh"),
                     live && !capturing && snapshot.state == SessionState::Error);
    }
    if (offers(OperationKind::Undo)) {
        appendAction(model.actions, OperationKind::Undo, translate("Undo"),
                     QStringLiteral("edit-undo"),
                     live && !capturing && !snapshot.lastText.isEmpty());
    }
    if (offers(OperationKind::CopyLast)) {
        appendAction(model.actions, OperationKind::CopyLast, translate("Copy"),
                     QStringLiteral("edit-copy"),
                     live && !snapshot.lastText.isEmpty());
    }
}

} // namespace

QString actionIdForKind(const OperationKind kind)
{
    for (const auto &token : kActionTokens) {
        if (token.kind == kind) {
            return QString::fromLatin1(token.id);
        }
    }
    return {};
}

bool kindForActionId(const QString &actionId, OperationKind &kind)
{
    for (const auto &token : kActionTokens) {
        if (actionId == QLatin1String(token.id)) {
            kind = token.kind;
            return true;
        }
    }
    return false;
}

VoiceAppletModel projectVoiceApplet(const ClientState clientState,
                                    const QString &clientReasonCode,
                                    const bool hasSnapshot, const Snapshot &snapshot,
                                    const quint32 levelPercent, const bool readGranted,
                                    const bool controlGranted,
                                    const bool showTranscript)
{
    VoiceAppletModel model;
    model.readGranted = readGranted;
    // Control without read would enable buttons over a projection we are not
    // allowed to have observed, so read gates both.
    model.controlGranted = readGranted && controlGranted;

    if (!readGranted) {
        model.phase = ServicePhase::Unavailable;
        model.stateId = QStringLiteral("unavailable");
        model.iconName = QStringLiteral("audio-input-microphone-muted");
        model.statusLabel = translate("Voice input is unavailable");
        model.diagnostic = translate("This applet is not permitted to observe voice "
                                     "input.");
        model.summaryLabel = translate("Voice");
        model.accessibleName = model.statusLabel;
        model.accessibleDescription = model.diagnostic;
        return model;
    }

    const bool ready = clientState == ClientState::Ready && hasSnapshot;
    model.phase = ready ? ServicePhase::Ready
                        : clientState == ClientState::Starting ? ServicePhase::Starting
                                                               : ServicePhase::Unavailable;
    const SessionState state = ready ? snapshot.state : SessionState::Unknown;
    model.revision = ready ? snapshot.revision : 0;
    model.enabled = ready && snapshot.enabled;
    model.commandMode = ready && snapshot.mode == CaptureMode::Command;
    model.capturing = ready && isCapturing(state);
    // A level outside capture is stale telemetry, never a rendered meter.
    model.levelPercent = model.capturing ? Services::Voice::clampLevelPercent(levelPercent)
                                         : 0;
    model.stateId = stateIdFor(state);
    model.iconName = iconNameFor(state, model.enabled);
    model.statusLabel = statusLabelFor(state, ready ? snapshot.mode : CaptureMode::Dictation);
    model.diagnostic = diagnosticFor(clientState, clientReasonCode, hasSnapshot, snapshot);
    model.summaryLabel = summaryLabelFor(model, state);

    if (ready) {
        model.providerLabel = snapshot.providerLabel.isEmpty() ? snapshot.providerId
                                                               : snapshot.providerLabel;
        model.languageLabel = snapshot.languageCode;
        model.microphoneLabel = snapshot.microphoneLabel;
        model.dictationShortcut = snapshot.dictationShortcut;
        model.commandShortcut = snapshot.commandShortcut;
        model.routeLabel = routeLabelFor(snapshot.lastRoute);
        model.transcriptIsPartial = showTranscript && !snapshot.partialText.isEmpty();
        if (showTranscript) {
            model.transcriptText = model.transcriptIsPartial ? snapshot.partialText
                                                             : snapshot.lastText;
        }
        buildActions(model, snapshot);
    }

    model.accessibleName = model.statusLabel;
    if (!model.diagnostic.isEmpty()) {
        model.accessibleDescription = model.diagnostic;
    } else if (model.capturing) {
        model.accessibleDescription = translate("Speaking now. Release the dictation "
                                                "shortcut to insert the text.");
    } else if (!model.dictationShortcut.isEmpty()) {
        model.accessibleDescription =
            translate("Hold %1 to dictate into the focused window.")
                .arg(model.dictationShortcut);
    } else {
        model.accessibleDescription = translate("Voice input is connected.");
    }
    return model;
}

} // namespace QindaQt::Shell::VoiceApplet
