// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/voice_applet/voice_request_state.h>

#include <QtCore/QCoreApplication>

namespace QindaQt::Shell::VoiceApplet {
namespace {

using Services::Voice::OperationKind;
using Services::Voice::OperationResult;
using Services::Voice::OperationStatus;
using Services::Voice::SessionState;
using Services::Voice::Snapshot;

QString translate(const char *text)
{
    return QCoreApplication::translate("VoiceApplet", text);
}

QString actionNoun(const OperationKind kind)
{
    switch (kind) {
    case OperationKind::StartDictation: return translate("Dictation");
    case OperationKind::StartCommand:   return translate("The voice command");
    case OperationKind::Finish:         return translate("Finishing the recording");
    case OperationKind::Cancel:         return translate("Cancelling the recording");
    case OperationKind::Retry:          return translate("The retry");
    case OperationKind::Undo:           return translate("The undo");
    case OperationKind::CopyLast:       return translate("The copy");
    case OperationKind::SetProvider:    return translate("The provider change");
    case OperationKind::SetEnabled:     return translate("The voice input switch");
    }
    return translate("The request");
}

RequestState refused(const OperationKind kind, const QString &feedback)
{
    return RequestState{.phase = RequestPhase::Failed,
                        .kind = kind,
                        .requestId = 0,
                        .initiatingRevision = 0,
                        .feedback = feedback};
}

bool captureInProgress(const SessionState state)
{
    return state == SessionState::Arming || state == SessionState::Listening
           || state == SessionState::Transcribing;
}

} // namespace

RequestState beginVoiceRequest(const Snapshot &snapshot, const bool hasSnapshot,
                               const OperationKind kind, const bool controlGranted)
{
    if (!controlGranted) {
        return refused(kind, translate("Voice input may not be controlled from here."));
    }
    if (!hasSnapshot) {
        return refused(kind, translate("No voice provider is connected."));
    }
    if (!Services::Voice::capabilityForKind(kind, snapshot.capabilities)) {
        return refused(kind,
                       translate("%1 is not supported by the current voice provider.")
                           .arg(actionNoun(kind)));
    }
    // Ordering rules the provider would also enforce, checked here so the panel
    // never offers a control whose only possible answer is a rejection.
    const bool capturing = captureInProgress(snapshot.state);
    const bool wantsCapture = kind == OperationKind::StartDictation
                              || kind == OperationKind::StartCommand;
    if (wantsCapture && capturing) {
        return refused(kind, translate("A recording is already in progress."));
    }
    if (wantsCapture && !snapshot.enabled) {
        return refused(kind, translate("Voice input is switched off."));
    }
    if ((kind == OperationKind::Finish || kind == OperationKind::Cancel) && !capturing) {
        return refused(kind, translate("There is no recording to finish."));
    }
    if (kind == OperationKind::Retry && snapshot.state != SessionState::Error) {
        return refused(kind, translate("There is nothing to retry."));
    }
    if ((kind == OperationKind::Undo || kind == OperationKind::CopyLast)
        && snapshot.lastText.isEmpty()) {
        return refused(kind, translate("Nothing has been dictated yet."));
    }
    return RequestState{.phase = RequestPhase::Pending,
                        .kind = kind,
                        .requestId = 0,
                        .initiatingRevision = snapshot.revision,
                        .feedback = {}};
}

RequestState applyVoiceResult(const RequestState &request, const OperationResult &result)
{
    if (!request.pending()) {
        return request;
    }
    if (result.requestId != request.requestId || result.kind != request.kind) {
        return request;
    }
    RequestState next = request;
    next.requestId = request.requestId;
    switch (result.status) {
    case OperationStatus::Succeeded:
        next.phase = RequestPhase::Succeeded;
        next.feedback.clear();
        break;
    case OperationStatus::Busy:
        next.phase = RequestPhase::Failed;
        next.feedback = translate("Voice input is busy with another request.");
        break;
    case OperationStatus::Rejected:
        next.phase = RequestPhase::Failed;
        next.feedback = translate("%1 was refused: %2")
                            .arg(actionNoun(request.kind), result.reasonCode);
        break;
    case OperationStatus::Failed:
        next.phase = RequestPhase::Failed;
        next.feedback = translate("%1 failed: %2")
                            .arg(actionNoun(request.kind), result.reasonCode);
        break;
    case OperationStatus::Uncertain:
        next.phase = RequestPhase::Uncertain;
        next.feedback = translate("%1 may or may not have happened. Check before "
                                  "repeating it.")
                            .arg(actionNoun(request.kind));
        break;
    }
    return next;
}

RequestState observeVoiceAuthority(const RequestState &request, const bool serviceAvailable)
{
    if (!request.pending() || serviceAvailable) {
        return request;
    }
    RequestState next = request;
    next.phase = RequestPhase::Uncertain;
    next.feedback = translate("%1 was interrupted when the voice provider went away.")
                        .arg(actionNoun(request.kind));
    return next;
}

} // namespace QindaQt::Shell::VoiceApplet
