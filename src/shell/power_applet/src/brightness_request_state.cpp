// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/power_applet/brightness_request_state.h>

#include <algorithm>
#include <utility>

namespace QindaQt::Shell::PowerApplet {
namespace {

using Power::OperationStatus;

// AGENT-GUARD: OperationStatus has a fixed quint32 underlying type. Range-
// check the raw value before the switch; an out-of-vocabulary status in a
// hostile reply must become typed Uncertain, never undefined behavior.
bool inVocabulary(const OperationStatus status, const quint32 maximum) {
  return static_cast<quint32>(status) <= maximum;
}

QString failureFeedback(const FailureKind kind) {
  switch (kind) {
  case FailureKind::Rejected:
    return QStringLiteral("The brightness change was rejected.");
  case FailureKind::Unsupported:
    return QStringLiteral("This keyboard backlight cannot be adjusted.");
  case FailureKind::Failed:
    return QStringLiteral("The brightness change failed.");
  case FailureKind::Busy:
    return QStringLiteral("The power service is busy; try again later.");
  case FailureKind::AuthenticationRequired:
    return QStringLiteral("Authentication is required to change brightness.");
  case FailureKind::Inhibited:
    return QStringLiteral("The brightness change is inhibited right now.");
  case FailureKind::ServiceUncertain:
    return QStringLiteral(
        "The brightness change may not have applied. Check the current "
        "value before retrying.");
  case FailureKind::LineageMismatch:
    return QStringLiteral(
        "The brightness state moved during the change. Check the current "
        "value before retrying.");
  case FailureKind::OwnerLost:
    return QStringLiteral(
        "The power service was replaced. Check the current value before "
        "retrying.");
  case FailureKind::MalformedReply:
    return QStringLiteral(
        "The power service returned an unreadable reply. Check the current "
        "value before retrying.");
  case FailureKind::None:
    break;
  }
  return QString();
}

BrightnessRequest terminalFailure(const BrightnessRequest &base,
                                  const FailureKind kind,
                                  QString reasonCode = {},
                                  QString diagnostic = {}) {
  BrightnessRequest request = base;
  request.phase = RequestPhase::Failed;
  request.failure = kind;
  request.reasonCode = std::move(reasonCode);
  request.diagnostic = std::move(diagnostic);
  request.feedback = failureFeedback(kind);
  return request;
}

BrightnessRequest uncertain(const BrightnessRequest &base,
                            const FailureKind kind,
                            QString reasonCode = {},
                            QString diagnostic = {}) {
  BrightnessRequest request = base;
  request.phase = RequestPhase::Uncertain;
  request.failure = kind;
  request.reasonCode = std::move(reasonCode);
  request.diagnostic = std::move(diagnostic);
  request.feedback = failureFeedback(kind);
  return request;
}

bool pending(const BrightnessRequest &request) {
  return request.phase == RequestPhase::Pending;
}

} // namespace

BrightnessRequest
beginKeyboardBrightnessRequest(const Power::Snapshot &snapshot,
                               const Power::Handle &device) {
  BrightnessRequest request;
  request.device = device;

  if (!device.isValid()) {
    return terminalFailure(request, FailureKind::Unsupported);
  }
  const bool capable =
      snapshot.capabilities.testFlag(Power::Capability::KeyboardBacklight);
  const auto devices = snapshot.keyboardBacklights;
  const auto entry = std::ranges::find_if(
      devices, [&device](const Power::KeyboardBacklight &candidate) {
        return candidate.handle.epoch == device.epoch &&
               candidate.handle.opaqueId == device.opaqueId;
      });
  if (!capable || entry == devices.cend() || !entry->canSet) {
    return terminalFailure(request, FailureKind::Unsupported);
  }
  // AGENT-GUARD: The initiating lineage is the observed generation at dispatch
  // time. A zero epoch cannot be a legal initiating generation, and the
  // result-matching rules below rely on it being nonzero.
  if (snapshot.epoch == 0) {
    return terminalFailure(request, FailureKind::Unsupported);
  }
  request.phase = RequestPhase::Pending;
  request.initiatingEpoch = snapshot.epoch;
  request.initiatingRevision = snapshot.revision;
  return request;
}

BrightnessRequest
applyOperationResult(const BrightnessRequest &request,
                     const Power::OperationResult &result) {
  if (!pending(request)) {
    // AGENT-GUARD: Terminal requests are immutable and never replay. A second
    // reply after completion must not resurrect or re-fail the request.
    return request;
  }
  if (!result.wireValid) {
    return uncertain(request, FailureKind::MalformedReply);
  }
  if (result.kind != Power::OperationKind::SetKeyboardBrightness) {
    // A reply for a different operation is stale for this request.
    return request;
  }
  const bool staleLineage =
      result.initiatingEpoch != request.initiatingEpoch ||
      result.initiatingRevision != request.initiatingRevision;
  if (staleLineage) {
    // AGENT-GUARD: A reply describing another dispatch attempt must neither
    // complete nor fail the live request; the live answer may still arrive.
    return request;
  }
  if (!inVocabulary(result.status, 7U)) {
    return uncertain(request, FailureKind::MalformedReply);
  }
  switch (result.status) {
  case OperationStatus::Succeeded:
    break;
  case OperationStatus::Rejected:
    return terminalFailure(request, FailureKind::Rejected,
                           result.reasonCode, result.diagnostic);
  case OperationStatus::Unsupported:
    return terminalFailure(request, FailureKind::Unsupported,
                           result.reasonCode, result.diagnostic);
  case OperationStatus::Failed:
    return terminalFailure(request, FailureKind::Failed, result.reasonCode,
                           result.diagnostic);
  case OperationStatus::Busy:
    return terminalFailure(request, FailureKind::Busy, result.reasonCode,
                           result.diagnostic);
  case OperationStatus::AuthenticationRequired:
    return terminalFailure(request, FailureKind::AuthenticationRequired,
                           result.reasonCode, result.diagnostic);
  case OperationStatus::Inhibited:
    return terminalFailure(request, FailureKind::Inhibited,
                           result.reasonCode, result.diagnostic);
  case OperationStatus::Uncertain:
    return uncertain(request, FailureKind::ServiceUncertain,
                     result.reasonCode, result.diagnostic);
  }
  // Success still requires the observed generation to be this service epoch
  // and at or after the initiating revision; anything earlier or foreign is
  // uncertain truth, not success.
  if (result.observedEpoch != request.initiatingEpoch ||
      result.observedRevision < request.initiatingRevision) {
    return uncertain(request, FailureKind::LineageMismatch,
                     result.reasonCode, result.diagnostic);
  }
  BrightnessRequest completed = request;
  completed.phase = RequestPhase::Succeeded;
  completed.failure = FailureKind::None;
  completed.feedback = QStringLiteral("Brightness changed.");
  return completed;
}

BrightnessRequest
observeGeneration(const BrightnessRequest &request,
                  const bool powerOwnerAvailable, const quint64 observedEpoch) {
  if (!pending(request)) {
    return request;
  }
  if (!powerOwnerAvailable) {
    return uncertain(request, FailureKind::OwnerLost);
  }
  if (observedEpoch != request.initiatingEpoch) {
    // The owner was replaced mid-request. Per the Power1 contract a
    // pending operation is uncertain; the user path must resnapshot and
    // this machine never replays the request automatically.
    return uncertain(request, FailureKind::OwnerLost);
  }
  return request;
}

} // namespace QindaQt::Shell::PowerApplet
